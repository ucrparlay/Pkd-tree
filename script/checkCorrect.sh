#!/bin/bash

Nodes=(1000000 5000000 8000000 10000000 50000000)

K=100
tester="checkCorrectParallel"
resFile="Correct.out"
dest="logger.in"
out="log.in"
: >${dest}
tag=2
count=1 # count the number of ok in the output
dim=5
queryTypes=(0)

# Paths=("/ssd0/zmen002/kdtree/uniform_float/" "/ssd0/zmen002/kdtree/ss_varden/")
Paths=("/ssd0/zmen002/kdtree/ss_varden/" "/ssd0/zmen002/kdtree/uniform_bigint/")

#* check node
for queryType in ${queryTypes[@]}; do
    for path in ${Paths[@]}; do
        for node in ${Nodes[@]}; do
            if [ ${queryType} -ge 1 ] && [ ${node} -gt 8000000 ]; then
                continue
            fi

            if [[ ${node} -ge 50000000 ]]; then
                K=10
            fi

            files_path="${path}${node}_${dim}"
            echo $files_path

            for file in "${files_path}/"*.in; do
                echo "------->${file}"
                ../build/${tester} -p ${file} -d ${dim} -k ${K} -t ${tag} -r 1 -q ${queryType} >>${dest}

                nc=$(grep -i -o "ok" ${dest} | wc -l)
                if [[ ${nc} -ne ${count} ]]; then
                    echo 'wrong'
                    exit
                fi
                count=$((count + 1))
            done

        done
    done
done

echo "OK, Well done :)"
echo "OK, Well done :)" >>"log.in"
