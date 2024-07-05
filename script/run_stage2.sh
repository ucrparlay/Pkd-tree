#!/bin/bash

set -x

# Solvers=("zdtree" "test")
Solvers=("test_pg")
 Node=(10000000 50000000 100000000 500000000)
# Node=(10000000)
Dim=(2 3)
# Dim=(2)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=2
k=100
onecore=0
insNum=2
queryType=3 # 001 011 111

resFile=""

for solver in ${Solvers[@]}; do
    exe="../build/${solver}"

    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res.out"
    elif [[ ${solver} == "test_pg" ]]; then
        resFile="pargeo.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal.out"
    elif [[ ${solver} == "zdtree" ]]; then
        resFile="zdtree.out"
        exe="/home/zmen002/pbbsbench_x/build/zdtree"
    elif [[ ${solver} == "cpam" ]]; then
        resFile="cpam.out"
        exe="/home/zmen002/CPAM_x/build/cpam_query"
    fi

    for dim in ${Dim[@]}; do
        for dataPath in "${!datas[@]}"; do
            for node in ${Node[@]}; do
                files_path="${dataPath}${node}_${dim}"
                log_path="${datas[${dataPath}]}${node}_${dim}"
                mkdir -p ${log_path}
                dest="${log_path}/${resFile}"
                : >${dest}
                echo ">>>${dest}"

                for ((i = 1; i <= ${insNum}; i++)); do
                    if [[ ${serial} == 1 ]]; then
                        PARLAY_NUM_THREADS=1 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 1 -q ${queryType} >>${dest}
                        continue
                    fi

                    tag=2723 # downsize_k=10, ins_ratio=10, tag=3
                    queryType=1 # kNN
                    k=10 # 10, 1
                    echo "Test for kNN with varying k from $k"
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

                    #tag=19 # downsize_k=0, ins_ratio=1, tag=3
                    #queryType=1 # kNN
                    #k=100 # fixed
                    #echo "Test for $k-NN with varying ins_ratio"
                    #PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}
                    #tag=51 # downsize_k=0, ins_ratio=3, tag=3
                    #echo "Test for $k-NN with varying ins_ratio"
                    #PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}
                    #tag=83 # downsize_k=0, ins_ratio=5, tag=3
                    #echo "Test for $k-NN with varying ins_ratio"
                    #PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

                    #tag=163 # downsize_k=0, ins_ratio=10, tag=3
                    #queryType=4 # rangeQuery
                    #k=1 # fixed, minimal
                    #echo "Test for rangeQuery with varying rect sizes"
                    #PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

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
done
