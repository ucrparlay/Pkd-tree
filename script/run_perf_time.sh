#!/bin/bash

echo " begin test >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/test -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0

echo " begin cgal >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0
PARLAY_NUM_THREADS=192 numactl -i all ./../build/cgal -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0

echo " begin zdtree >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
PARLAY_NUM_THREADS=192 numactl -i all ./../../pbbsbench_x/build/zdtree -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 0 -r 3 -i 0 -k 10
