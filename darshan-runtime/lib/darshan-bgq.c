/*
 * Copyright (C) 2015 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include "darshan-runtime-config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "uthash.h"
#include "darshan.h"
#include "darshan-bgq-log-format.h"

#ifdef __bgq__
#include <spi/include/kernel/location.h>
#include <spi/include/kernel/process.h>
#include <firmware/include/personality.h>
#endif

/*
 * Simple module which captures BG/Q hardware specific information about 
 * the job.
 * 
 * This module does not intercept any system calls. It just pulls data
 * from the personality struct at initialization.
 */


/*
 * Global runtime struct for tracking data needed at runtime
 */
struct bgq_runtime
{
    struct darshan_bgq_record record;
};

static struct bgq_runtime *bgq_runtime = NULL;
static pthread_mutex_t bgq_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* the instrumentation_disabled flag is used to toggle functions on/off */
static int instrumentation_disabled = 0;

/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* internal helper functions for the "NULL" module */
void bgq_runtime_initialize(void);

/* forward declaration for module functions needed to interface with darshan-core */
static void bgq_begin_shutdown(void);
static void bgq_get_output_data(void **buffer, int *size);
static void bgq_shutdown(void);
static void bgq_setup_reduction(darshan_record_id *shared_recs,int *shared_rec_count,void **send_buf,void **recv_buf,int *rec_size);
static void bgq_record_reduction_op(void* infile_v,void* inoutfile_v,int *len,MPI_Datatype *datatype);

/* macros for obtaining/releasing the "NULL" module lock */
#define BGQ_LOCK() pthread_mutex_lock(&bgq_runtime_mutex)
#define BGQ_UNLOCK() pthread_mutex_unlock(&bgq_runtime_mutex)

/*
 * Function which updates all the counter data
 */
static void capture(struct darshan_bgq_record *rec)
{
#ifdef __bgq__
    Personality_t person;
    int r;

    rec->counters[BGQ_CSJOBID] = Kernel_GetJobID();
    rec->counters[BGQ_RANKSPERNODE] = Kernel_ProcessCount();

    rec->counters[BGQ_INODES] = MPIX_IO_node();

    r = Kernel_GetPersonality(&person, sizeof(person));
    if (r == 0)
    {
        rec->counters[BGQ_NNODES] = ND_TORUS_SIZE(person.Network_Config);
        rec->counters[BGQ_ANODES] = person.Network_Config.Anodes;
        rec->counters[BGQ_BNODES] = person.Network_Config.Bnodes;
        rec->counters[BGQ_CNODES] = person.Network_Config.Cnodes;
        rec->counters[BGQ_DNODES] = person.Network_Config.Dnodes;
        rec->counters[BGQ_ENODES] = person.Network_Config.Enodes;
        rec->counters[BGQ_TORUSENABLED] =
            ((person.Network_Config.NetFlags & ND_ENABLE_TORUS_DIM_A) << 0) |
            ((person.Network_Config.NetFlags & ND_ENABLE_TORUS_DIM_B) << 1) |
            ((person.Network_Config.NetFlags & ND_ENABLE_TORUS_DIM_C) << 2) |
            ((person.Network_Config.NetFlags & ND_ENABLE_TORUS_DIM_D) << 3) |
            ((person.Network_Config.NetFlags & ND_ENABLE_TORUS_DIM_E) << 4);

        rec->counters[BGQ_DDRPERNODE] = person.DDR_Config.DDRSizeMB;
    }
#endif

    rec->rank = my_rank;
    rec->fcounters[BGQ_F_TIMESTAMP] = darshan_core_wtime();

    return;
}

/**********************************************************
 * Internal functions for manipulating BGQ module state *
 **********************************************************/

void bgq_runtime_initialize()
{
    /* struct of function pointers for interfacing with darshan-core */
    struct darshan_module_funcs bgq_mod_fns =
    {
        .begin_shutdown = bgq_begin_shutdown,
        .setup_reduction = bgq_setup_reduction, 
        .record_reduction_op = bgq_record_reduction_op, 
        .get_output_data = bgq_get_output_data,
        .shutdown = bgq_shutdown
    };
    int mem_limit;
    char *recname = "darshan-internal-bgq";

    BGQ_LOCK();

    /* don't do anything if already initialized or instrumenation is disabled */
    if(bgq_runtime || instrumentation_disabled)
        return;

    /* register the "NULL" module with the darshan-core component */
    darshan_core_register_module(
        DARSHAN_BGQ_MOD,
        &bgq_mod_fns,
        &mem_limit,
        NULL);

    /* return if no memory assigned by darshan-core */
    if(mem_limit == 0)
    {
        instrumentation_disabled = 1;
        return;
    }

    /* no enough memory to fit bgq module */
    if (mem_limit < sizeof(*bgq_runtime))
    {
        instrumentation_disabled = 1;
        return;
    }

    /* initialize module's global state */
    bgq_runtime = malloc(sizeof(*bgq_runtime));
    if(!bgq_runtime)
    {
        instrumentation_disabled = 1;
        return;
    }
    memset(bgq_runtime, 0, sizeof(*bgq_runtime));

    darshan_core_register_record(
        recname,
        strlen(recname),
        1,
        DARSHAN_POSIX_MOD,
        &bgq_runtime->record.f_id,
        &bgq_runtime->record.alignment);

    DARSHAN_MPI_CALL(PMPI_Comm_rank)(MPI_COMM_WORLD, &my_rank);

    capture(&bgq_runtime->record);

    BGQ_UNLOCK();

    return;
}

/* Perform any necessary steps prior to shutting down for the "NULL" module. */
static void bgq_begin_shutdown()
{
    BGQ_LOCK();

    /* In general, we want to disable all wrappers while Darshan shuts down. 
     * This is to avoid race conditions and ensure data consistency, as
     * executing wrappers could potentially modify module state while Darshan
     * is in the process of shutting down. 
     */
    instrumentation_disabled = 1;

    BGQ_UNLOCK();

    return;
}

/* Pass output data for the "BGQ" module back to darshan-core to log to file. */
static void bgq_get_output_data(
    void **buffer,
    int *size)
{

    /* Just set the output buffer to point at the array of the "BGQ" module's
     * I/O records, and set the output size according to the number of records
     * currently being tracked.
     */
    if ((bgq_runtime) && (my_rank == 0))
    {
        *buffer = &bgq_runtime->record;
        *size = sizeof(struct darshan_bgq_record);
    }
    else
    {
        *buffer = NULL;
        *size   = 0;
    }

    return;
}

/* Shutdown the "BGQ" module by freeing up all data structures. */
static void bgq_shutdown()
{
    if (bgq_runtime)
    {
        free(bgq_runtime);
        bgq_runtime = NULL;
    }

    return;
}

static void bgq_setup_reduction(
    darshan_record_id *shared_recs,
    int *shared_rec_count,
    void **send_buf,
    void **recv_buf,
    int *rec_size)
{
    int i;
    int found;

    for (i = 0; i < *shared_rec_count; i++)
    {
        if (shared_recs[i] == bgq_runtime->record.f_id)
        {
            found = 1;
            break;
        }
    }

    if (found)
    {
        printf("found bgq shared record\n");
        *rec_size = sizeof(struct darshan_bgq_record);
        *shared_rec_count = 1;
        *send_buf = &bgq_runtime->record;
        *recv_buf = &bgq_runtime->record;
    }

    return;
}

static void bgq_record_reduction_op(
    void* infile_v,
    void* inoutfile_v,
    int* len,
    MPI_Datatype *datatype)
{
    int i;
    int j;
    struct darshan_bgq_record *infile = infile_v;
    struct darshan_bgq_record *inoutfile = inoutfile_v;

    for (i = 0; i<*len; i++)
    {
        for (j = 0; j < BGQ_NUM_INDICES; j++)
        {
            if (infile->counters[j] != inoutfile->counters[j])
            {
                // unexpected
                fprintf(stderr,
                        "%lu counter mismatch: %d [%lu] [%lu]\n",
                        infile->f_id,
                        j,
                        infile->counters[j],
                        inoutfile->counters[j]);
            }
        }
        infile++;
        inoutfile++;
    }

    return;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
