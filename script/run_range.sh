#!/bin/bash

set -x

# Solvers=("zdtree" "test")
Solvers=("test_pg")
#Node=(10000000 50000000 100000000)
Node=(100000000)
#Node=(10000000)
# Dim=(3)
#Dim=(2 3 5 7 9)
#Dim=(5 7 9)
# Dim=(9)
Dim=(2)
declare -A datas
datas["/mnt/dappur/Workspace/kdtree/KDtree/data/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

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

                    # del_mode=0 (unused), ins_mode=0 (unused), build_mode=1 (normal build), seg_mode=0 (unused)
                    # downsize_k=0, ins_ratio=0(unused), tag=0 (build only)
                    tag=65536
                    queryType=4 # RangeQuery
                    k=0 # not used
                    echo "Test 100M for range query" >>${dest}
                    echo "dim=${dim} BHL Tree:" >>${dest}
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 1 >>${dest}
                    echo "dim=${dim} Log Tree:" >>${dest}
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

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
