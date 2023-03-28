#!/bin/bash

# Solvers=("cgal")
Solvers=("test" "cgal")
path="../benchmark/craft_var_dim/100000_"
# Dims=("2" "3" "5" "7" "9" "10" "12" "15")
Dims=("18" "20" "25")
T="1200"
K="100"

resFile=""

for solver in ${Solvers[@]}
    do
    #* decide output file
    if [[ ${solver} == "test" ]]; then
        resFile="res.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal_res.out"
    fi 

    #* main body
    for dim in ${Dims[@]}
    do
        files_path="${path}${dim}"
        : > "${files_path}/${resFile}"
        echo ${solver}" <- ${files_path}"

        for file in "${files_path}/"*.in
        do
            file_name="${file##*"/"}"
            timeout ${T} ../build/${solver} ${file} ${K} >> "${files_path}/${resFile}"
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
done

