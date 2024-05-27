#!/bin/bash

# NOTE: find the process id by script name
# pgrep -f script_name
# sleep 330m
# ./kill_script.sh $1

# NOTE: varden insert
# PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/ss_varden/1000000000_3/1.in -t 0 -s 0 -d 3 -q 8192 -r 1 -i 1 -k 10 | tee data/serialBatch_origin/Parallel_insert_varden.log
PARLAY_NUM_THREADS=1 numactl -i all ./../build/test -p /data3/zmen002/kdtree/ss_varden/1000000000_3/1.in -t 0 -s 0 -d 3 -q 8192 -r 1 -i 1 -k 10 | tee data/serialBatch_origin/serial_insert_varden.log

# NOTE: varden delete
# PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/ss_varden/1000000000_3/1.in -t 0 -s 0 -d 3 -q 16384 -r 3 -i 1 -k 10 | tee data/serialBatch_origin/Parallel_delete_varden.out
PARLAY_NUM_THREADS=1 numactl -i all ./../build/test -p /data3/zmen002/kdtree/ss_varden/1000000000_3/1.in -t 0 -s 0 -d 3 -q 16384 -r 3 -i 1 -k 10 | tee data/serialBatch_origin/serial_delete_varden.out

# NOTE: uniform insert
# PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/uniform/1000000000_3/1.in -t 0 -s 0 -d 3 -q 8192 -r 1 -i 1 -k 10 | tee data/serialBatch_origin/Parallel_insert_uniform.log
PARLAY_NUM_THREADS=1 numactl -i all ./../build/test -p /data3/zmen002/kdtree/uniform/1000000000_3/1.in -t 0 -s 0 -d 3 -q 8192 -r 1 -i 1 -k 10 | tee data/serialBatch_origin/serial_insert_uniform.log

# NOTE: uniform delete
# PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/uniform/1000000000_3/1.in -t 0 -s 0 -d 3 -q 16384 -r 3 -i 1 -k 10 | tee data/serialBatch_origin/Parallel_delete_uniform.out
PARLAY_NUM_THREADS=1 numactl -i all ./../build/test -p /data3/zmen002/kdtree/uniform/1000000000_3/1.in -t 0 -s 0 -d 3 -q 16384 -r 3 -i 1 -k 10 | tee data/serialBatch_origin/serial_delete_uniform.out
