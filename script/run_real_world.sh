#!/bin/bash
set -o xtrace
# Solvers=("cgal" "test")
Solvers=("test" "cgal")
DataPath="/data3/zmen002/kdtree/geometry"
declare -A file2Dims
file2Dims["HT"]="10"
file2Dims["Household"]="7"
file2Dims["CHEM"]="16"
file2Dims["GeoLifeNoScale"]="3"
file2Dims["Cosmo50"]="3"
file2Dims["osm"]="2"

tag=0
k=100
onecore=0
insNum=0
readFile=0
# queryType=$((2#1)) # 1110000
queryType=8 # 1110000
type="real_world_range_query_perf"
resFile=""

for solver in ${Solvers[@]}; do
    exe="../build/${solver}"

    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res_${type}.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal_${type}.out"
    fi

    log_path="../benchmark/real_world"
    mkdir -p ${log_path}
    dest="${log_path}/${resFile}"
    : >${dest}
    echo ">>>${dest}"

    for filename in "${!file2Dims[@]}"; do

        perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ${exe} -p "${DataPath}/${filename}.in" -k ${k} -t ${tag} -d ${file2Dims[${filename}]} -q ${queryType} -i ${readFile} -s 0 -r 1 >>${dest} 2>&1

    done
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
