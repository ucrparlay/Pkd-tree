#!/bin/bash

Solvers=("zdtree" "test" "cgal")
DataPath="/data9/zmen002/kdtree/geometry"
declare -A file2Dims
file2Dims["Cosmo50"]="3"
file2Dims["GeoLifeNoScale"]="3"
file2Dims["Household"]="7"
file2Dims["HT"]="10"

tag=0
k=100
onecore=0
insNum=2
readFile=0
# queryType=$((2#1)) # 1110000
queryType=1 # 1110000
type="real_world"
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

  log_path="../benchmark/real_world"
  mkdir -p ${log_path}
  dest="${log_path}/${resFile}"
  : >${dest}
  echo ">>>${dest}"

  for filename in "${!file2Dims[@]}"; do
    if [ ${solver} == "zdtree" ] && [ ${file2Dims[${filename}]} -gt "3" ]; then
      continue
    fi
    echo ${filename}

    PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${DataPath}/${filename}.in" -k ${k} -t ${tag} -d ${file2Dims[${filename}]} -q ${queryType} -i ${readFile} >>${dest}

  done
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
