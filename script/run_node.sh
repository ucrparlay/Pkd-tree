#!/bin/bash

Solvers=( "cgal" "test")
Node=(10000000 50000000 100000000 500000000 1000000000)
path="../benchmark/craft_var_node/"
dim=3
T=1800
k=100
wrap=16

resFile=""

for solver in ${Solvers[@]}; do
    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal_res.out"
    fi

    #* main body
    for node in ${Node[@]}; do
        files_path="${path}${node}_${dim}"
        mkdir -p ${files_path}
        dest="${files_path}/${resFile}"
        : >${dest}
        echo ">>>${dest}"

        for ((i = 1; i <= 3; i++)); do
            # file_name="${file##*"/"}"
            ((node++))
            timeout ${T} ../build/test ${node} ${dim} ${k} >>${dest}
            retval=$?
            if [ ${retval} -eq 124 ]; then
                echo -e "${node}_${dim}.in -1 -1 -1 -1" >>${dest}
                echo "timeout ${node}_${dim}"
            else
                echo "finish ${node}_${dim}"
            fi
        done
    done
done
