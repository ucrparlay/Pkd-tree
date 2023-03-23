#!/bin/bash

path="../benchmark/craft_var_dim/100000_"
Dims=("2" "3" "5" "7" "9" "10" "12" "15")
# Dims=("12" "15")
tester="cgal"
resFile="cgal_res.out"
T="1200"

for dim in ${Dims[@]}
do
    files_path="${path}${dim}"
    : > "${files_path}/${resFile}"
    echo "-------${files_path}"

    for file in "${files_path}/"*.in
    do
        file_name="${file##*"/"}"
        timeout ${T} ../build/${tester} ${file} >> "${files_path}/${resFile}"
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
