#!/bin/bash

echo " begin VARDEN ... "
echo " begin test varden >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# echo " begin cgal varden >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
#

# NOTE: BELOW IS UNIFORM

echo " begin UNIFORM ... "
echo " begin test uniform >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/test -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
#
# echo " begin cgal uniform >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
perf stat -B ./../build/cgal -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
