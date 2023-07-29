#!/bin/bash

Solvers=("zdtree" "test")
Cores=(1 4 8 16 24 48 96)
# Node=(100000)
Node=(100000000 500000000)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/scalability/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/scalability/"

tag=0
dim=3
k=100
onecore=0
insNum=1

resFile=""

for solver in ${Solvers[@]}; do
    #* decide output file
    if [[ ${solver} == "test" ]]; then
        if [[ ${tag} == -1 ]]; then
            continue
            resFile="res_serial.out"
        else
            resFile="res_parallel.out"
            # resFile="res_parallel_one_core.out"
        fi
    elif [[ ${solver} == "cgal" ]]; then
        if [[ ${tag} == -1 ]]; then
            continue
            resFile="cgal_res_serial.out"
        else
            resFile="cgal_res_parallel.out"
        fi
    elif [[ ${solver} == "zdtree" ]]; then
        if [[ ${tag} == -1 ]]; then
            continue
        else
            resFile="zdtree.out"
        fi
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
                    PARLAY_NUM_THREADS=${core} taskset -c 0-95:${step} ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 2 >>${dest}
                done
            done
        done
    done
done
