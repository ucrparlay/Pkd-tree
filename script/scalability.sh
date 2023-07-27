#!/bin/bash

Solvers=("zdtree" "test")
Cores=(1 8 16 24 48 96)
# Node=(100000)
Node=(10000000 50000000 100000000 500000000)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/scalability/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/scalability/"

Tag=(0 1)
dim=3
k=100
onecore=0
insNum=2

resFile=""

for solver in ${Solvers[@]}; do
    for tag in ${Tag[@]}; do
        #* decide output file
        if [[ ${solver} == "test" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="res_serial.out"
            else
                resFile="res_parallel.out"
                # resFile="res_parallel_one_core.out"
            fi
        elif [[ ${solver} == "cgal" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="cgal_res_serial.out"
            else
                resFile="cgal_res_parallel.out"
            fi
        elif [[ ${solver} == "zdtree" ]]; then
            if [[ ${tag} == 0 ]]; then
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
                    for ((i = 1; i <= ${insNum}; i++)); do
                        PARLAY_NUM_THREADS=${core} numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 3 >>${dest}
                    done
                done
            done
        done
    done
done
