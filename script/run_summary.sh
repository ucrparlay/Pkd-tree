#!/bin/bash
# {
#     sleep 210m
#     kill $$
# } &

Solvers=("zdtree" "cgal" "test")
# Solvers=("zdtree")
# Node=(10000000 50000000 100000000 500000000)
Node=(100000000)
Dim=(2 3)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=2
k=10
onecore=0
insNum=2
# queryType=1
queryType=$((2#1010)) # 1110000
echo $queryType
type="summary"

resFile=""

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
          PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>${dest}

        done
      done
    done
  done
done
