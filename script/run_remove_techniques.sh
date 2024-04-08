#!/bin/bash
# set -o xtrace

cd "../build/" || exit
rm -rf *

run_test() {
	varden_path="/data3/zmen002/kdtree/ss_varden/1000000000_3/1.in"
	uniform_path="/data3/zmen002/kdtree/uniform/1000000000_3/1.in"
	PARLAY_NUM_THREADS=1 ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=192 ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=1 ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=192 ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
}

run_cache() {
	varden_path="/data3/zmen002/kdtree/ss_varden/1000000000_3/1.in"
	uniform_path="/data3/zmen002/kdtree/uniform/1000000000_3/1.in"
	PARLAY_NUM_THREADS=1 perf stat -e cache-misses ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=192 perf stat -e cache-misses ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=1 perf stat -e cache-misses ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=192 perf stat -e cache-misses ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
}

remove_technique() {
	echo "----------------------------------------"
	cmake -DDEBUG=OFF -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DNTH_ELEMENT="$1" -DONE_LEVEL="$2" -DSAMPLE="$3" -DTEST_CACHE="$4" ..
	make test
	run_test
	echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}

test_cache_usage() {
	echo "----------------------------------------"
	cmake -DDEBUG=OFF -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DNTH_ELEMENT="$1" -DONE_LEVEL="$2" -DSAMPLE="$3" -DTEST_CACHE="$4" ..
	make test
	run_cache
	echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}

# NOTE: NTH_ELEMENT | ONE_LEVEL | SAMPLE | TEST_CACHE
# remove_technique "ON" "OFF" "ON" "OFF"
# remove_technique "ON" "ON" "ON" "OFF"
# remove_technique "OFF" "ON" "OFF" "OFF"
# remove_technique "ON" "ON" "OFF" "OFF"

echo ">>>>>>>>>>>>>>>>begin cache>>>>>>>>>>>>>>>>"
test_cache_usage "ON" "OFF" "ON" "ON"
test_cache_usage "ON" "ON" "ON" "ON"
test_cache_usage "OFF" "ON" "OFF" "ON"
test_cache_usage "ON" "ON" "OFF" "ON"
