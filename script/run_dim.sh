#!/bin/bash

Solvers=("test" "cgal")
node=1000000
path="../benchmark/craft_var_dim/"
Dims=(2 3 5 7 9)
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
    for dim in ${Dims[@]}; do
        files_path="${path}${node}_${dim}"
        mkdir -p ${files_path}
        dest="${files_path}/${resFile}"
        : >${dest}
        echo ">>>${dest}"

        for ((i = 1; i <= 3; i++)); do
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
