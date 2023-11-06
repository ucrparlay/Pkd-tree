#!/bin/bash

set -x

# Solvers=("zdtree" "test")
Solvers=("test_pg")
# Node=(10000000 50000000 100000000)
Node=(10000000)
# Dim=(2 3)
Dim=(2)
declare -A datas
datas["/mnt/dappur/Workspace/KDtree/data/ss_varden/"]="../benchmark/ss_varden/"
# datas["/mnt/dappur/Workspace/KDtree/data/uniform/"]="../benchmark/uniform/"

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

                    # del_mode=0 (unused), ins_mode=0 (unused), build_mode=1 (wp), seg_mode=0 (unused)
                    # downsize_k=10, ins_ratio=0(unused), tag=0 (build only)
                    tag=68096
                    queryType=1 # kNN
                    k=10
                    echo "Test for $k-NN with build once"
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

                    # del_mode=3 (none), ins_mode=2 (seg mode), build_mode=2 (empty), seg_mode=1 (10%)
                    # downsize_k=10, ins_ratio=0(unused), tag=2
                    tag=52566530
                    queryType=1 # kNN
                    k=10
                    echo "Test for $k-NN with incremental insertion"
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

                    # del_mode=2 (seg mode), ins_mode=1 (all), build_mode=1 (wp), seg_mode=1 (10%)
                    # downsize_k=10, ins_ratio=0(unused), tag=2
                    tag=34675202
                    queryType=1 # kNN
                    k=10
                    echo "Test for $k-NN with incremental deletion"
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}

                    # del_mode=4 (partial), ins_mode=4 (partial), build_mode=1 (wp), seg_mode=0 (unused)
                    # downsize_k=0 (unused), ins_ratio=1-10 (10%-100%), tag=2
                    queryType=0 # no query
                    k=10 # unused
                    echo "Test for insertion and deletion with batch size 10%-100%"
                    tags=(71368722 71368738 71368754 71368770 71368786 71368802 71368818 71368834 71368850 71368866)
                    for tag in ${tags[@]}; do
                        PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -T 2 >>${dest}
                    done

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
