#!/bin/bash

path="../benchmark/craft_var_node/"
# Nodes=("2000000")
Nodes=("10000" "50000" "100000" "500000" "800000" "1000000" "2000000")
Dims="5"
tester="test"
resFile="res.out"
T="1200"

for node in ${Nodes[@]}
do
    files_path="${path}${node}_${Dims}"
    : > "${files_path}/${resFile}"
    echo "-------${files_path}"

    for file in "${files_path}/"*.in
    do
        file_name="${file##*"/"}"
        timeout ${T} ../build/${tester} $file >> "${files_path}/${resFile}"
        retval=$?
        if [ ${retval} -eq 124 ]
        then
            echo -e "${file_name} -1 -1" >> "${files_path}/${resFile}"
            echo "timeout ${file_name}"
        else
            echo "finish ${file_name}"
        fi
    done
done
