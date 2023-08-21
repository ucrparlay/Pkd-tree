#!/bin/bash

Solvers=("cgal" "test")
# Node=(100000000)
Node=(10000000 50000000 100000000 500000000)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=2
dim=3
k=100
onecore=0
insNum=2
queryType=7 # 001 011 111

resFile=""

for solver in ${Solvers[@]}; do
    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal.out"
    elif [[ ${solver} == "zdtree" ]]; then
        resFile="zdtree.out"
    fi

    for dataPath in "${!datas[@]}"; do
        for node in ${Node[@]}; do
            files_path="${dataPath}${node}_${dim}"
            log_path="${datas[${dataPath}]}${node}_${dim}"
            mkdir -p ${log_path}
            dest="${log_path}/${resFile}"
            : >${dest}
            echo ">>>${dest}"

            exe="../build/${solver}"
            if [[ ${solver} == "zdtree" ]]; then
                export LD_PRELOAD=/usr/local/lib/libjemalloc.so.2
                exe="/home/zmen002/pbbsbench_x/benchmarks/nearestNeighbors/octTree/neighbors"
            fi

            for ((i = 1; i <= ${insNum}; i++)); do
                if [[ ${serial} == 1 ]]; then
                    PARLAY_NUM_THREADS=1 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 1 -q ${queryType} >>${dest}
                    continue
                fi
                PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>${dest}

                retval=$?
                if [ ${retval} -eq 124 ]; then
                    echo -e "${node}_${dim}.in ${T} -1 -1 -1 -1" >>${dest}
                    echo "timeout ${node}_${dim}"
                else
                    echo "finish ${node}_${dim}"
                fi
            done
        done
    done
done
