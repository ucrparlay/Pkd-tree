#!/bin/bash
path="../benchmark/craft_var_node_integer/"
node="10000000"
Wrap=("1" "2" "4" "8" "16" "32" "64" "128")
K=("1" "10" "100" "1000")
Dims="3"
tester="test"
resFile="res_"
T="1800"

#* run integer
for wrap in ${Wrap[@]}
do
    for k in ${K[@]}
    do
        files_path="${path}${node}_${Dims}"
        mkdir -p ${files_path}
        dest="${files_path}/${resFile}${wrap}_${k}.out"
        : > ${dest}
        echo ">>>${dest}"
        
        for ((i=1;i<=2;i++));
        do
            timeout ${T} ../build/${tester} ${node} ${Dims} ${k} ${wrap} >> ${dest}
            retval=$?
            if [ ${retval} -eq 124 ]
            then
                echo -e "${node}_${Dims} -1 -1 -1 -1 -1" >> ${dest}
                echo "timeout ${file_name}"
            else
                echo "finish ${file_name}"
            fi
        done
    done
done
