#!/bin/bash
# set -o xtrace

cd "../build/" || exit
rm -rf *

run_test() {
	varden_path="/ssd0/zmen002/kdtree/ss_varden/10000000_3/1.in"
	uniform_path="/ssd0/zmen002/kdtree/uniform/10000000_3/1.in"
	PARLAY_NUM_THREADS=1 ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=192 ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=1 ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
	PARLAY_NUM_THREADS=192 ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
}

run_cache() {
	varden_path="/data3/zmen002/kdtree/ss_varden/1000000000_3/1.in"
	uniform_path="/data3/zmen002/kdtree/uniform/1000000000_3/1.in"
	perf stat -e cache-misses PARLAY_NUM_THREADS=1 ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	perf stat -e cache-misses PARLAY_NUM_THREADS=192 ./test -p ${uniform_path} -d 3 -t 0 -q 0 -r 2 -i 0
	perf stat -e cache-misses PARLAY_NUM_THREADS=1 ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
	perf stat -e cache-misses PARLAY_NUM_THREADS=192 ./test -p ${varden_path} -d 3 -t 0 -q 0 -r 2 -i 0
}

# NOTE: lambda=6, sampling
echo "----------------------------------------"
cmake -DDEBUG=OFF -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DNTH_ELEMENT=ON -DONE_LEVEL=OFF -DSAMPLE=ON -DTEST_CACHE=OFF ..
make test
run_test
echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# NOTE: lambda=1, sampling
echo "----------------------------------------"
cmake -DDEBUG=OFF -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DNTH_ELEMENT=ON -DONE_LEVEL=ON -DSAMPLE=ON -DTEST_CACHE=OFF ..
make test
run_test
echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# NOTE: lambda=1, parallel median
echo "----------------------------------------"
cmake -DDEBUG=OFF -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DNTH_ELEMENT=OFF -DONE_LEVEL=ON -DSAMPLE=OFF -DTEST_CACHE=OFF ..
make test
run_test
echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# NOTE: lambda=1, serial median
echo "----------------------------------------"
cmake -DDEBUG=OFF -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DNTH_ELEMENT=ON -DONE_LEVEL=ON -DSAMPLE=OFF -DTEST_CACHE=OFF ..
make test
run_test
echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
