#!/bin/bash

set -x

# Solvers=("zdtree" "test")
#Solvers=("bhltree" "logtree")
Solvers=("bhltree")
#Node=(10000000 50000000 100000000)
#Node=(10000000) #1e7
#Node=(100000000) #1e8
Node=(1000000000) #1e9
#Dim=(2 3)
#Dim=(5 7 9)
Dim=(3)
declare -A datas
datas["/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
#datas["/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

declare -a commands=(
    #"numactl -i all"                   # 192 threads
    #"taskset -c 0-95 numactl -i all"   # 96 threads
    #"taskset -c 0-95:2 numactl -i all" # 48 threads
    #"taskset -c 0-95:4 numactl -i all" # 24 threads
    #"taskset -c 0-63:4 numactl -i all" # 16 threads
    #"taskset -c 0-31:4 numactl -i all" # 8 threads
    #"taskset -c 0-15:4 numactl -i all" # 4 threads
    #"taskset -c 0-7:4 numactl -i all"  # 2 threads
    "taskset -c 0-3:4"                 # 1 thread
)

#declare -a threadset=(192 96 48 24 16 8 4 2 1)
declare -a threadset=(1)

tag=1
k=100
onecore=0
insNum=1
queryType=3 # 001 011 111

resFile=""

for solver in ${Solvers[@]}; do
    exe="../build/${solver}"

    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res.out"
    elif [[ ${solver} == "bhltree" ]]; then
        resFile="bhltree.out"
        exe="../build/test_pg -T 1"
    elif [[ ${solver} == "logtree" ]]; then
        resFile="logtree.out"
        exe="../build/test_pg -T 2"
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
                for threads in $threadset; do
                    : >${dest}.t${threads}.out
                    echo ">>>${dest}.t${threads}.out"
                done

                for ((i = 1; i <= ${insNum}; i++)); do
                    if [[ ${serial} == 1 ]]; then
                        PARLAY_NUM_THREADS=1 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 1 -q ${queryType} >>${dest}
                        continue
                    fi

                    # del_mode=4 (partial), ins_mode=4 (partial), build_mode=0 (unit), seg_mode=0 (unused)
                    # downsize_k=0 (unused), ins_ratio=1-10 (1%-10%), tag=2
                    queryType=0 # no query
                    k=10 # unused
                    echo "Test for insertion and deletion with batch size 1%, and build"
                    #tags=(71368722 71368738 71368754 71368770 71368786 71368802 71368818 71368834 71368850 71368866)
                    tag=$((71368722-65536))
                    #for threads in 1 2 4 8 16 24 48 96 192; do
                    #for threads in 192 96 48 24 16 8 4 2 1; do
                    for th_idx in "${!threadset[@]}"; do
                        threads=${threadset[${th_idx}]}
                        if [ $threads -eq 1 ]; then
                            rounds=1
                        else
                            #rounds=3
                            rounds=1
                        fi
                        echo "PARLAY_NUM_THREADS=${threads} ${commands[${th_idx}]}"
                        PARLAY_NUM_THREADS=${threads} ${commands[${th_idx}]} ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r ${rounds} >>${dest}.t${threads}.out
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
