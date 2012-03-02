#!/bin/bash
#
# Run configure for runtime and utils

basedir=$PWD
status=0
fcount=0
runtime_result=""
util_result=""
thedate=$(date)

cd build/darshan-runtime
../../darshan-runtime/configure --prefix=$basedir/install --with-mem-align=16 --with-jobid-env=DARSHAN_JOBID --with-log-path=$basedir/logs --with-log-path-by-env DARSHAN_LOGPATH CC=/usr/local/mpich/mpich-1.4.1p1/bin/mpicc > configure.out 2>&1
runtime_status=$?
if [ $runtime_status -ne 0 ]; then
  fcount=$((fcount+1));
  runtime_result="<error type='$runtime_status' message='configure failed' />"
fi

cd ../darshan-util
../../darshan-util/configure --prefix=$basedir/install > configure.out 2>&1
util_status=$?
if [ $util_status -ne 0 ]; then
  fcount=$((fcount+1));
  util_result="<error type='$util_status' message='configure failed' />"
fi

echo "
<testsuites>
  <testsuite name='configure' tests='2' failures='$fcount' time='$thedate'>
    <testcase name='darshan-runtime' time='$thedate'>
    </testcase>
    $runtime_result
    <testcase name='darshan-util' time='$thedate'>
    $util_result
    </testcase>
  </testsuite>
</testsuites>
" > configure-result.xml
exit $fcount
