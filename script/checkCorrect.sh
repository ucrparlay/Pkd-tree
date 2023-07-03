#!/bin/bash

path="../benchmark/craft_var_dim_integer/"
# Nodes=(10000 50000 100000 500000 1000000 5000000)
Nodes=(500000)
Dims=(2 3 5 7 9)
# Dims=(5)
K=100
tester="checkCorrectParallel"
resFile="Correct.out"

for node in ${Nodes[@]}; do
    for dim in ${Dims[@]}; do
        if [ ${node} -ge "10000000" ]; then
            dim="3"
        fi

        files_path="${path}${node}_${dim}"
        mkdir -p ${files_path}
        : >"${files_path}/${resFile}"
        echo "-------${files_path}"

        for ((i = 1; i <= 3; i++)); do
            ((nodes++))
            ../build/${tester} ${node} ${dim} >>"${files_path}/${resFile}"
        done
        # for file in "${files_path}/"*.in; do
        #     file_name="${file##*"/"}"
        #     ../build/${tester} ${file} ${K} >>"${files_path}/${resFile}"
        # done
    done
done
