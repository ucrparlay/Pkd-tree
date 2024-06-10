#!/bin/bash

# NOTE: find the process id by script name
# pgrep -f script_name

# sleep 330m
# ./kill_script.sh $1

# PARLAY_NUM_THREADS=192 timeout 3700s numactl -i all ./../../pbbsbench_x/build/zdtree -p /data3/zmen002/kdtree/uniform/100000000_2/1.in -d 2 -t 0 -k 10 -q 2048 -i 0 -r 3 | tee ../benchmark/real_world/osm_year_zdtree.out
#
# PARLAY_NUM_THREADS=192 timeout 3700s numactl -i all ./../../pbbsbench_x/build/zdtree -p /data3/zmen002/kdtree/uniform/100000000_2/1.in -d 2 -t 0 -k 10 -q 4096 -i 0 -r 3 | tee ../benchmark/real_world/osm_month_zdtree.out

PARLAY_NUM_THREADS=192 INBALANCE_RATIO=30 numactl -i all ../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -k 10 -t 0 -d 3 -q 32768 -i 0 -r 1 | tee "data/inba30_od_remote_multi.log"
PARLAY_NUM_THREADS=192 INBALANCE_RATIO=40 numactl -i all ../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -k 10 -t 0 -d 3 -q 32768 -i 0 -r 1 | tee "data/inba40_od_remote_multi.log"
PARLAY_NUM_THREADS=192 INBALANCE_RATIO=48 numactl -i all ../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -k 10 -t 0 -d 3 -q 32768 -i 0 -r 1 | tee "data/inba48_od_remote_multi.log"
PARLAY_NUM_THREADS=192 INBALANCE_RATIO=49 numactl -i all ../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -k 10 -t 0 -d 3 -q 32768 -i 0 -r 1 | tee "data/inba49_od_remote_multi.log"
# INBALANCE_RATIO=20 INBA_QUERY=0 INBA_BUILD=1 ../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -k 10 -t 0 -d 3 -q 1024 -i 0 -r 1 | tee "data/inba49_od.log"
