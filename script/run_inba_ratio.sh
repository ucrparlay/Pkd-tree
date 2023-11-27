#!/bin/bash
# {
#     sleep 210m
#     kill $$
# } &

Solvers=("test")
# Node=(10000000 50000000 100000000 500000000)
Node=(100000000)
Inba=(1 2 3 4 5 6 10 20 30 40 45 46 47 48 49 50)
Dim=(3)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=0
k=10
onecore=0
insNum=1
queryType=1024 # 001 011 111
# queryType=$((2#1111000000)) # 1110000
echo $queryType

resFile="inba_ratio.out"

for solver in ${Solvers[@]}; do
    exe="../build/${solver}"

    for dim in ${Dim[@]}; do
        for dataPath in "${!datas[@]}"; do
            for node in ${Node[@]}; do
                files_path="${dataPath}${node}_${dim}"
                dest="data/${resFile}"
                : >${dest}
                echo ">>>${dest}"

                for ratio in ${Inba[@]}; do
                    PARLAY_NUM_THREADS=192 INBALANCE_RATIO=${ratio} numactl -i all ${exe} -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>${dest}

                done
            done
        done
    done
done
