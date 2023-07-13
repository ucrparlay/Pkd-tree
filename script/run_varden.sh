#!/bin/bash

Solvers=("cgal" "test")
Node=(10000000 50000000 100000000)
path="../benchmark/ss_varden/"
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
            fi
        elif [[ ${solver} == "cgal" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="cgal_res_serial.out"
            else
                continue
                resFile="cgal_res_parallel.out"
            fi
        fi

        #* node main body
        for node in ${Node[@]}; do
            files_path="${path}${node}_${dim}"
            mkdir -p ${files_path}
            dest="${files_path}/${resFile}"
            : >${dest}
            echo ">>>${dest}"

            for file in "${files_path}/"*.in; do
                # echo ${file}
                timeout ${T} ../build/${solver} ${file} ${K} ${tag} >>${dest}

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
