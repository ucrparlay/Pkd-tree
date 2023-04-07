#!/bin/bash

# Solvers=("test")
# Solvers=("test" "cgal")
node=10000000
path="../benchmark/craft_var_dim_integer/${node}_"
Dims=("2" "3" "5" "7" "9")
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
    for dim in ${Dims[@]}
    do
        files_path="${path}${dim}"
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
    
#     #* main body
#     for dim in ${Dims[@]}
#     do
#         files_path="${path}${dim}"
#         : > "${files_path}/${resFile}"
#         echo ${solver}" <- ${files_path}"
        
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

