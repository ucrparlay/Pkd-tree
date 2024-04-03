#!/bin/bash

echo: " begin test >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0

$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0

echo: " begin cgal >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
perf stat -e cache-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0

$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
$(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 0 -r 1 -i 0
