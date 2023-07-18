#!/bin/bash

Solvers=("cgal" "test")
Node=(10000000 50000000 100000000)
path="/ssd0/zmen002/kdtree/ss_varden/"
SerialTag=(0 1)
dim=3
T=3600
K=100
wrap=16

resFile=""

for solver in ${Solvers[@]}; do
    for tag in ${SerialTag[@]}; do
        #* decide output file
        if [[ ${solver} == "test" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="res_serial.out"
            else
                resFile="res_parallel.out"
                # resFile="res_parallel_one_core.out"
            fi
        elif [[ ${solver} == "cgal" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="cgal_res_serial.out"
            else
                continue
                resFile="cgal_res_parallel.out"
            fi
        elif [[ ${solver} == "zdtree" ]]; then
            if [[ ${tag} == 1 ]]; then
                resfile="zdtree.out"
            else
                continue
            fi
        fi

        #* node main body
        for node in ${Node[@]}; do
            files_path="${path}${node}_${dim}"
            log_path="../benchmark/ss_varden/${node}_${dim}"
            mkdir -p ${log_path}
            dest="${log_path}/${resFile}"
            : >${dest}
            echo ">>>${dest}"

            for ((i = 1; i <= 2; i++)); do
                timeout ${T} numactl -i all ../build/${solver} "${files_path}/${i}.in" ${k} ${tag} >>${dest}

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
