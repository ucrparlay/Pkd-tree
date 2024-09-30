#!/bin/bash
dest="logs/perf_inba_vary_pre.out"
: >$dest
Node=(10000000 20000000 50000000 100000000 200000000 500000000 1000000000)
Solver=("test" "cgal")
Bench=("ss_varden" "uniform")

for bench in "${Bench[@]}"; do
	if [ "$bench" == "varden" ]; then
		echo " begin VARDEN ... " >>$dest
	else
		echo " begin UNIFORM ... " >>$dest
	fi

	for solver in "${Solver[@]}"; do
		for node in "${Node[@]}"; do
			perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ./../build/"$solver" -p /data/zmen002/kdtree/"${bench}"/"$node"_3/1.in -d 3 -t 0 -q 8 -r 1 -i 0 >>${dest} 2>&1
		done
	done
done
