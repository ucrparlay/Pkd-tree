#!/bin/bash

Nodes=(1000000 5000000 8000000 10000000)
Dims=(2 3 5 7 9)
K=100
tester="checkCorrectParallel"
resFile="Correct.out"
dest="logger.in"
out="log.in"
: >${dest}
tag=2
count=1

# Paths=("/ssd0/zmen002/kdtree/uniform_bigint/")
Paths=("/ssd0/zmen002/kdtree/ss_varden/" "/ssd0/zmen002/kdtree/uniform_bigint/")

#* check node
for path in ${Paths[@]}; do
    for node in ${Nodes[@]}; do
        dim=5

        files_path="${path}${node}_${dim}"

        for file in "${files_path}/"*.in; do
            echo "------->${file}"
            ../build/${tester} -p ${file} -d ${dim} -k ${K} -t ${tag} -r 2 >>${dest}

            nc=$(grep -i -o "ok" ${dest} | wc -l)
            if [[ ${nc} -ne ${count} ]]; then
                echo 'wrong'
                exit
            fi
            count=$((count + 1))
        done

    done
done

echo "finish node test"
exit

#* check dim
for node in ${Nodes[@]}; do
    if [ ${node} -ne 5000000 ]; then
        continue
    fi

    path="../benchmark/craft_var_dim_integer/"
    for dim in ${Dims[@]}; do
        files_path="${path}${node}_${dim}"
        echo "------->${files_path}"

        for ((i = 1; i <= 3; i++)); do
            nodeVar=$((${node} + ${i}))
            ../build/${tester} -n ${nodeVar} -d ${dim} -t ${tag} -r 2 >>${dest}

            nc=$(grep -i -o "ok" ${dest} | wc -l)
            if [[ ${nc} -ne ${count} ]]; then
                echo 'wrong'
                exit
            fi
            count=$((count + 1))
        done
    done
done

echo "Well done :)"
echo "OK, Well done :)" >>"log.in"
