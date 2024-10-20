#!/bin/bash
set -o xtrace
# Solvers=("test" "cgal")
Solvers=("test")
DataPath="/data/legacy/data3/zmen002/kdtree/geometry"
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
# QueryTypes=(7 15)
QueryTypes=(15)

for queryType in ${QueryTypes[@]}; do
    log_path="logs"
    perf_path="/localdata/zmen002/kdtree/perfs"
    if [[ ${queryType} == 7 ]]; then
        resFile="perf_real_gen_serial.out"
    elif [[ ${queryType} == 15 ]]; then
        resFile="perf_real_total_serial.out"
    fi
    dest="${log_path}/${resFile}"
    : >${dest}
    echo ">>>${dest}"

    for solver in ${Solvers[@]}; do
        exe="../build/${solver}"
        echo ">>> ${solver}" >>${dest}

        for filename in "${!file2Dims[@]}"; do
            if [[ ${solver} == "cgal" ]]; then
                if [ ${filename} == "Household" ] || [ ${filename} == "GeoLifeNoScale" ]; then
                    continue
                fi
                func_name="search"
            elif [[ ${solver} == "test" ]]; then
                func_name="range_query_recursive_serial"
            fi

            perf_data_name="${perf_path}/${solver}_${filename}_perf_serial.data"
            perf_report_name="${perf_path}/${solver}_${filename}_perf_serial.report"

            ctl_dir=/tmp/

            ctl_fifo=${ctl_dir}perf_ctl.fifo
            test -p ${ctl_fifo} && unlink ${ctl_fifo}
            mkfifo ${ctl_fifo}
            exec {ctl_fd}<>${ctl_fifo}

            ctl_ack_fifo=${ctl_dir}perf_ctl_ack.fifo
            test -p ${ctl_ack_fifo} && unlink ${ctl_ack_fifo}
            mkfifo ${ctl_ack_fifo}
            exec {ctl_fd_ack}<>${ctl_ack_fifo}

            perf record -s -D -1 --control fd:${ctl_fd},${ctl_fd_ack} -o ${perf_data_name} -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses ${exe} -p "${DataPath}/${filename}.in" -k ${k} -t ${tag} -d ${file2Dims[${filename}]} -q ${queryType} -i ${readFile} -s 0 -r 1 -pcf ${ctl_fd} -pcaf ${ctl_fd_ack}

            exec {ctl_fd_ack}>&-
            unlink ${ctl_ack_fifo}

            exec {ctl_fd}>&-
            unlink ${ctl_fifo}

            perf report --stdio --input=${perf_data_name} >${perf_report_name}

            echo -n "${filename} " >>${dest}
            res=$(grep -E "Samples|Event count|\\b${func_name}\\b" "${perf_report_name}" | awk '/Event count/ {print $NF}')
            # res=$(grep -E "Samples|Event count|\\b${func_name}\\b" "${perf_report_name}" | awk '/Event count/ {print $NF} /'"${func_name}"'/ {gsub("%", "", $1); print $1}' | awk '{if($1>100){print; kp=1}else if(kp){print; kp=0}}')
            # res=$(echo "${res}" | awk '{v[NR]=$1;} END {for(i=1;i<=NR;i+=2){if(i+1<=NR){res=(v[i]*v[i+1])/100; printf "%d\n", res;}}}')
            echo -n "${res}" | paste -sd ' ' >>${dest}
        done
    done
done

# NOTE: parse the output from the perf report
# grep -E 'Samples|Event count|\bsearch\b' tmp.txt | awk '$1!="0.00%"' | awk '/Event count/ {print $NF} /search/ {gsub("%", "", $1); print $1}' |  awk '{v[NR]=$1;} END {for(i=1;i<=NR;i+=2){if(i+1<=NR){res=(v[i]*v[i+1])/100; printf "%d\n", res;}}}' | paste -sd ' '
