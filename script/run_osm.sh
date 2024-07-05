#!/bin/bash

set -x

# Solvers=("zdtree" "test")
Solvers=("bhltree" "logtree")
#Solvers=("logtree")
#Node=(10000000 50000000 100000000)
Node=(0)
#Node=(10000000)
#Dim=(2 3)
#Dim=(2 3 5 7 9)
#Dim=(2 3 5 9)
Dim=(2)
declare -A datas
#datas["/data/zmen002/kdtree/real_world/osm/year/"]="../benchmark/osm/year/"
datas["/data/zmen002/kdtree/real_world/osm/month/"]="../benchmark/osm/month/"

tag=2
k=100
onecore=0
#insNum=2
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
                files_path="${dataPath}"
                log_path="${datas[${dataPath}]}"
                mkdir -p ${log_path}
                dest="${log_path}/${resFile}"
                : >${dest}
                echo ">>>${dest}"

                for ((i = 1; i <= ${insNum}; i++)); do
                    queryType=0 # not in use
                    k=10 # not in use
                    echo "Test for update insertion on osm by year"
                    # tag[30]=1 (special mode), (tag&0xf)==1 (osm by year), tag[4..16]=2014, tag[16..28]=2023
                    #tag=1206353377
                    #PARLAY_NUM_THREADS=192 numactl -i all ${exe} -i "${files_path}" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r 1 >>${dest}
                    # tag[30]=1 (special mode), (tag&0xf)==2 (osm by month), tag[4..16]=2014, tag[16..28]=2023
                    tag=1206353378
                    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -i "${files_path}" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r 1 >>${dest}

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
