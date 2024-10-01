#!/bin/bash
dest="logs/perf_inba_vary.out"
:$dest
# Node=(10000000 20000000 50000000 100000000 200000000 500000000 1000000000)
# Solver=("test" "cgal")
Node=(10000000 20000000)
Solver=("test")
Bench=("varden" "uniform")

for bench in "${Bench[@]}"; do
	if [ "$bench" == "varden" ]; then
		echo " begin VARDEN ... " >>$dest
	else
		echo " begin UNIFORM ... " >>$dest
	fi

	for solver in "${Solver[@]}"; do
		for node in "${Node[@]}"; do
			perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/"$solver" -p /data/zmen002/kdtree/"${bench}"/"$node"_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0 >>${dest}
		done
	done
done

# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# echo " begin cgal varden >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/ss_varden/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
#

# NOTE: BELOW IS UNIFORM

# echo " begin UNIFORM ... "
# echo " begin test uniform >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/test -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/test -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
#
# echo " begin cgal uniform >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/cgal -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0

# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/10000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/20000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/50000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/100000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/200000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/500000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
# $(which time) -v ./../build/cgal -p /data/zmen002/kdtree/uniform/1000000000_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0
