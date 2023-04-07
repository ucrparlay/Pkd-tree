#!/bin/bash

Node=("10000000" "50000000" "100000000" "500000000" "1000000000")
path="../benchmark/craft_var_node_integer/"
dim="3"
ID=("0" "1")
T="1800"
k="10"
wrap="16"

resFile=""

for id in ${ID[@]}
do
    #* decide output file
    if [[ ${id} == "0" ]]; then
        resFile="res_bounded_queue.out"
        elif [[ ${id} == "1" ]]; then
        resFile="res_karray_queue.out"
    fi
    
    #* main body
    for node in ${Node[@]}
    do
        files_path="${path}${node}_${dim}"
        mkdir -p ${files_path}
        dest="${files_path}/${resFile}"
        : > ${dest}
        echo ">>>${dest}"
        
        for ((i=1;i<=2;i++));
        do
            # file_name="${file##*"/"}"
            timeout ${T} ../build/test ${node} ${dim} ${k} ${wrap} ${id} >> ${dest}
            retval=$?
            if [ ${retval} -eq 124 ]
            then
                echo -e "${node}_${dim}.in -1 -1 -1 -1" >> ${dest}
                echo "timeout ${node}_${dim}"
            else
                echo "finish ${node}_${dim}"
            fi
        done
    done
done
# for solver in ${Solvers[@]}
# do
#     #* decide output file
#     if [[ ${solver} == "test" ]]; then
#         resFile="res.out"
#         elif [[ ${solver} == "cgal" ]]; then
#         resFile="cgal_res.out"
#     fi
#     for node in ${Nodes[@]}
#     do
#         files_path="${path}${node}_${Dims}"
#         : > "${files_path}/${resFile}"
#         echo "-------${files_path}"
        
#         for file in "${files_path}/"*.in
#         do
#             file_name="${file##*"/"}"
#             timeout ${T} ../build/${solver} ${file} ${K} >> "${files_path}/${resFile}"
#             retval=$?
#             if [ ${retval} -eq 124 ]
#             then
#                 echo -e "${file_name} -1 -1" >> "${files_path}/${resFile}"
#                 echo "timeout ${file_name}"
#             else
#                 echo "finish ${file_name}"
#             fi
#         done
#     done
# done