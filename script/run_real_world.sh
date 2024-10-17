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
QueryTypes=(7 15)

for queryType in ${QueryTypes[@]}; do
    log_path="logs"
    if [[ ${queryType} == 7 ]]; then
        resFile="perf_real_gen.out"
    elif [[ ${queryType} == 15 ]]; then
        resFile="perf_real_total.out"
    fi
    dest="${log_path}/${resFile}"
    : >${dest}
    echo ">>>${dest}"

    for solver in ${Solvers[@]}; do
        exe="../build/${solver}"
        for filename in "${!file2Dims[@]}"; do
            if [[ ${solver} == "cgal" ]]; then
                if [ ${filename} == "Household" ] || [ ${filename} == "GeoLifeNoScale" ]; then
                    continue
                fi
            fi

            perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ${exe} -p "${DataPath}/${filename}.in" -k ${k} -t ${tag} -d ${file2Dims[${filename}]} -q ${queryType} -i ${readFile} -s 0 -r 1 >>${dest} 2>&1

        done
    done
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time

# NOTE: parse out the datas from results generated from perf stat
# grep -Eo '([0-9, ]+) (cycles|instructions|cache-references|cache-misses|branch-instructions|branch-mi sses):u' logs/perf_real_gen.out | awk '{gsub(/,/, "", $1); print $1}' | paste -sd ' ' | awk '{for (i=1; i<=NF; i++) {printf "%s ", $i; if (i%6==0) printf "\n"}} END {if (NF%6!=0) printf "\n"}' #
