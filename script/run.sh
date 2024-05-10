#!/bin/bash

# NOTE: find the process id by script name
# pgrep -f script_name

# sleep 330m
# ./kill_script.sh $1

# PARLAY_NUM_THREADS=192 timeout 3700s numactl -i all ./../../pbbsbench_x/build/zdtree -p /data3/zmen002/kdtree/uniform/100000000_2/1.in -d 2 -t 0 -k 10 -q 2048 -i 0 -r 3 | tee ../benchmark/real_world/osm_year_zdtree.out
#
# PARLAY_NUM_THREADS=192 timeout 3700s numactl -i all ./../../pbbsbench_x/build/zdtree -p /data3/zmen002/kdtree/uniform/100000000_2/1.in -d 2 -t 0 -k 10 -q 4096 -i 0 -r 3 | tee ../benchmark/real_world/osm_month_zdtree.out

# PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/ss_varden/1000000000_3/1.in -t 0 -s 0 -d 3 -q 8192 -r 1 -i 1 -k 10 | tee data/vardenSerialInsert.out

# PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/ss_varden/1000000000_3/1.in -t 0 -s 0 -d 3 -q 16384 -r 1 -i 1 -k 10 | tee data/vardenSerialDelete.out

PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/uniform/1000000000_3/1.in -t 0 -s 0 -d 3 -q 8192 -r 1 -i 1 -k 10 | tee data/uniformSerialInsert.out

PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data3/zmen002/kdtree/uniform/1000000000_3/1.in -t 0 -s 0 -d 3 -q 16384 -r 1 -i 1 -k 10 | tee data/uniformSerialDelete.out
