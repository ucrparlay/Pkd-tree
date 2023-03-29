#!/bin/bash

path="../benchmark/craft_var_node/"
# Nodes=("2000000")
Nodes=("10000" "50000" "100000" "500000" "800000" "1000000" "2000000")
Dims="5"
tester="checkCorrect"
resFile="Correct.out"

for node in ${Nodes[@]}
do
    files_path="${path}${node}_${Dims}"
    : > "${files_path}/${resFile}"
    echo "-------${files_path}"
    
    for file in "${files_path}/"*.in
    do
        file_name="${file##*"/"}"
        ../build/${tester} $file >> "${files_path}/${resFile}"
    done
done

# path="../benchmark/craft_var_dim/100000_"
# Dims=("2" "3" "5" "7" "9" "10" "12" "15")
# for dim in ${Dims[@]}
# do
#     files_path="${path}${dim}"
#     : > "${files_path}/${resFile}"
#     echo "-------${files_path}"

#     for file in "${files_path}/"*.in
#     do
#         file_name="${file##*"/"}"
#         ../build/${tester} ${file} >> "${files_path}/${resFile}"
#     done
# done