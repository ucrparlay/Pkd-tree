#!/bin/bash
set -o xtrace
# Solvers=("cgal" "test")
Solvers=("test" "cgal")
DataPath="/data3/zmen002/kdtree/geometry"

tag=0
k=10
onecore=0
insNum=0
readFile=0
queryType=2048 # 1110000
type="real_world_update"
resFile=""

log_path="../benchmark/real_world"
mkdir -p ${log_path}
dest="${log_path}/batch_update.out"
: >${dest}
echo ">>>${dest}"

for solver in ${Solvers[@]}; do
    exe="../build/${solver}"

    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res_${type}.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal_${type}.out"
    elif [[ ${solver} == "zdtree" ]]; then
        resFile="zdtree_${type}.out"
        exe="/home/zmen002/pbbsbench_x/build/zdtree"
    fi

    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "/data/legacy/data3/zmen002/kdtree/ss_varden/100000000_2/1.in" -d 2 -k ${k} -t ${tag} -q ${queryType} -i ${readFile} -s 0 -r 3 >>${dest}
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
