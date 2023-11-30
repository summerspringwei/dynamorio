#!/bin/bash

set -xe

CBENCH_HOME=${HOME}/Software/cBench/
DYNAMO_HOME=${HOME}/Software/dynamorio/build/

# cd ${CBENCH_HOME}/automotive_bitcount/src
# cd ${CBENCH_HOME}/automotive_qsort1/src
cd ${CBENCH_HOME}/automotive_susan_c/src

$DYNAMO_HOME/./bin64/drrun -c $DYNAMO_HOME/api/bin/libbbcount.so -- ./a.out ../../automotive_susan_data/1.pgm output_large.corners.pgm -c 2>&1 | tee basic_block_dump.txt
# $DYNAMO_HOME/./bin64/drrun -c $DYNAMO_HOME/api/bin/libbbcount.so -- bash __run 1 1
