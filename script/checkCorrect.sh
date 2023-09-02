#!/bin/bash

Nodes=(1000000 5000000 8000000 10000000 50000000)

K=100
tester="checkCorrectParallel"
resFile="Correct.out"
dest="logger.in"
out="log.in"
: >${dest}
tag=2
count=1
dim=5
queryTypes=(0 1 2)

Paths=("/ssd0/zmen002/kdtree/uniform_float/" "/ssd0/zmen002/kdtree/ss_varden/")

#* check node
for queryType in ${queryTypes[@]}; do
    for path in ${Paths[@]}; do
        for node in ${Nodes[@]}; do
            if [ ${queryType} -ge 1 ] && [ ${node} -gt 8000000 ]; then
                continue
            fi

            files_path="${path}${node}_${dim}"
            echo $files_path

            for file in "${files_path}/"*.in; do
                echo "------->${file}"
                ../build/${tester} -p ${file} -d ${dim} -k ${K} -t ${tag} -r 2 -q ${queryType} >>${dest}

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

echo "Well done :)"
echo "OK, Well done :)" >>"log.in"
