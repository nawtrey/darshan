/*
 * Copyright (C) 2015 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __DARSHAN_STDIO_LOG_FORMAT_H
#define __DARSHAN_STDIO_LOG_FORMAT_H

/* current log format version, to support backwards compatibility */
#define DARSHAN_STDIO_VER 1

#define STDIO_COUNTERS \
    /* count of fopens */\
    X(STDIO_OPENS) \
    /* maximum byte (offset) written */\
    X(STDIO_MAX_BYTE_WRITTEN) \
    /* total bytes written */ \
    X(STDIO_BYTES_WRITTEN) \
    /* number of writes */ \
    X(STDIO_WRITES) \
    /* maximum byte (offset) read */\
    X(STDIO_MAX_BYTE_READ) \
    /* total bytes read */ \
    X(STDIO_BYTES_READ) \
    /* number of reads */ \
    X(STDIO_READS) \
    /* count of seeks */\
    X(STDIO_SEEKS) \
    /* count of flushes */\
    X(STDIO_FLUSHES) \
    /* end of counters */\
    X(STDIO_NUM_INDICES)

#define STDIO_F_COUNTERS \
    /* timestamp of first open */\
    X(STDIO_F_OPEN_START_TIMESTAMP) \
    /* timestamp of last open completion */\
    X(STDIO_F_OPEN_END_TIMESTAMP) \
    /* timestamp of first close */\
    X(STDIO_F_CLOSE_START_TIMESTAMP) \
    /* timestamp of last close completion */\
    X(STDIO_F_CLOSE_END_TIMESTAMP) \
    /* timestamp of first write */\
    X(STDIO_F_WRITE_START_TIMESTAMP) \
    /* timestamp of last write completion */\
    X(STDIO_F_WRITE_END_TIMESTAMP) \
    /* timestamp of first read */\
    X(STDIO_F_READ_START_TIMESTAMP) \
    /* timestamp of last read completion */\
    X(STDIO_F_READ_END_TIMESTAMP) \
    /* cumulative meta time */\
    X(STDIO_F_META_TIME) \
    /* cumulative write time */\
    X(STDIO_F_WRITE_TIME) \
    /* cumulative read time */\
    X(STDIO_F_READ_TIME) \
    /* end of counters */\
    X(STDIO_F_NUM_INDICES)

#define X(a) a,
/* integer counters for the "STDIO" module */
enum darshan_stdio_indices
{
    STDIO_COUNTERS
};

/* floating point counters for the "STDIO" module */
enum darshan_stdio_f_indices
{
    STDIO_F_COUNTERS
};
#undef X

/* the darshan_stdio_record structure encompasses the high-level data/counters
 * which would actually be logged to file by Darshan for the "STDIO"
 * module. This logs the following data for each record:
 *      - a corresponding Darshan record identifier
 *      - the rank of the process responsible for the record
 *      - integer I/O counters (operation counts, I/O sizes, etc.)
 *      - floating point I/O counters (timestamps, cumulative timers, etc.)
 */
struct darshan_stdio_record
{
    darshan_record_id f_id;
    int64_t rank;
    int64_t counters[STDIO_NUM_INDICES];
    double fcounters[STDIO_F_NUM_INDICES];
};

#endif /* __DARSHAN_STDIO_LOG_FORMAT_H */
