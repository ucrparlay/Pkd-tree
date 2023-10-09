#!/bin/bash

Solvers=("test")
Cores=(1 4 8 16 24 48 96)
# Node=(100000)
Node=(100000000 500000000)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/scalability/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/scalability/"

tag=2
dim=3
k=100
onecore=0
insNum=1

resFile=""

for solver in ${Solvers[@]}; do
    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal.out"
    elif [[ ${solver} == "zdtree" ]]; then
        resFile="zdtree.out"
        exe="/home/zmen002/pbbsbench_x/build/zdtree"
    elif [[ ${solver} == "cpam" ]]; then
        resFile="cpam.out"
        exe="/home/zmen002/CPAM_x/build/cpam_query"
    fi

    exe="../build/${solver}"
    if [[ ${solver} == "zdtree" ]]; then
        export LD_PRELOAD=/usr/local/lib/libjemalloc.so.2
        exe="/home/zmen002/pbbsbench_x/benchmarks/nearestNeighbors/octTree/neighbors"
    fi

    for core in ${Cores[@]}; do
        for dataPath in "${!datas[@]}"; do
            for node in ${Node[@]}; do
                logPath=${datas[${dataPath}]}

                files_path="${dataPath}${node}_${dim}"
                log_path="${logPath}${node}_${dim}"
                mkdir -p ${log_path}
                dest="${log_path}/${core}_${resFile}"
                : >${dest}
                echo ">>>${dest}"

                step=$((96 / core))
                echo $step
                for ((i = 1; i <= ${insNum}; i++)); do
                    #! NUMA control would slow down the serial process dramatically.
                    #! e.g., in zd tree, the sort would be 30% slower and 40% slower for build the tree
                    PARLAY_NUM_THREADS=${core} taskset -c 0-95:${step} numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 2 >>${dest}
                done
            done
        done
    done
done
