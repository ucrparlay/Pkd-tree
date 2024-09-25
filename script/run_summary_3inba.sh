#!/bin/bash
# {
# 	sleep 10s
# 	# kill $$
# 	pkill -P $$
# } &
set -o xtrace
# Solvers=("rtree" "test" "cgal")
# Solvers=("test" "cgal")
Solvers=("test")
# Node=(100000000 1000000000)
Node=(1000000000)
Dim=(2 3 5 9)
Inba=(3 10 30)
declare -A datas
datas["/data/legacy/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data/legacy/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=2
k=10
insNum=1
# queryType=$((2#1)) # 1110000
queryType=$((2#1001)) # 1110000
echo $queryType
type="summary"
rounds=3

resFile=""

for solver in "${Solvers[@]}"; do
    exe="../build/${solver}"

    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res_${type}.out"
    fi

    for dim in "${Dim[@]}"; do
        for dataPath in "${!datas[@]}"; do
            for node in "${Node[@]}"; do
                files_path="${dataPath}${node}_${dim}"
                log_path="${datas[${dataPath}]}${node}_${dim}"
                mkdir -p "${log_path}"
                dest="${log_path}/${resFile}"
                : >"${dest}"
                echo ">>>${dest}"

                for ratio in "${Inba[@]}"; do
                    for ((i = 1; i <= insNum; i++)); do
                        export INBALANCE_RATIO=${ratio}
                        export PARLAY_NUM_THREADS=192
                        numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r ${rounds} -i 1 -s 1 >>"${dest}"
                    done
                done
            done
        done
    done
done
