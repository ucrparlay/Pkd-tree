#!/bin/bash
path="../benchmark/craft_var_node_integer/"
node="100000000"
Wrap=("1" "2" "4" "8" "16" "32" "64" "128" "256" "512" "1024")
K=("1" "10" "100" "1000" "10000" "100000")
Dims="3"
tester="test"
resFile="res_"
T="1200"

#* run integer
for wrap in ${Wrap[@]}
do
    for k in ${K[@]}
    do
        files_path="${path}${node}_${Dims}"
        mkdir -p ${files_path}
        dest="${files_path}/${resFile}${k}_${wrap}.out"
        : > ${dest}
        echo ">>>${dest}"
        
        for ((i=1;i<=3;i++));
        do
            timeout ${T} ../build/${tester} ${node} ${Dims} ${K} ${wrap} >> ${dest}
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

#* run file
# for wrap in ${Wrap[@]}
# do
#     files_path="${path}${node}_${Dims}"
#     : > "${files_path}/${resFile}${wrap}.out"
#     echo "-------${files_path}_${wrap}"

#     for file in "${files_path}/"*.in
#     do
#         file_name="${file##*"/"}"
#         timeout ${T} ../build/${tester} ${file} ${K} ${wrap} >> "${files_path}/${resFile}${wrap}.out"
#         retval=$?
#         if [ ${retval} -eq 124 ]
#         then
#             echo -e "${file_name} -1 -1" >> "${files_path}/${resFile}"
#             echo "timeout ${file_name}"
#         else
#             echo "finish ${file_name}"
#         fi
#     done
# done
