#!/bin/bash

Nodes=(1000000 5000000 10000000 50000000)
Dims=(2 3 5 7 9)
K=100
tester="checkCorrectParallel"
resFile="Correct.out"
: >"log.in"

#* check node
for node in ${Nodes[@]}; do
    dim=5
    path="/ssd0/zmen002/kdtree/ss_varden/"

    files_path="${path}${node}_${dim}"
    mkdir -p ${files_path}
    : >"${files_path}/${resFile}"

    for file in "${files_path}/"*.in; do
        echo "------->${file}"
        ../build/${tester} ${file} ${K} >>"${files_path}/${resFile}"
    done

    #* verify correctness
    if grep -c "wrong" "${files_path}/${resFile}" || grep -c "Assertion" "${files_path}/${resFile}"; then
        echo 'wrong'
        exit
    fi
done

echo "finish node test"

#* check dim
for node in ${Nodes[@]}; do
    if [ ${node} -ne 5000000 ]; then
        continue
    fi

    path="../benchmark/craft_var_dim_integer/"
    for dim in ${Dims[@]}; do
        files_path="${path}${node}_${dim}"
        mkdir -p ${files_path}
        : >"${files_path}/${resFile}"
        echo "------->${files_path}"

        for ((i = 1; i <= 3; i++)); do
            nodeVar=$((${node} + ${i}))
            ../build/${tester} ${nodeVar} ${dim} >>"${files_path}/${resFile}"
        done

        #* verify correctness
        if grep -c "wrong" "${files_path}/${resFile}" || grep -c "Assertion" "${files_path}/${resFile}"; then
            echo 'wrong'
            exit
        fi
    done
done

echo "Well done :)"
echo "OK, Well done :)" >>"log.in"
