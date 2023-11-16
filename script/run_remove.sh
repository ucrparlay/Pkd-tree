#!/bin/bash

# Solvers=("test")
Solvers=("cgal")
Node=(100000000)
Dim=(2 3)
datas=("/data9/zmen002/kdtree/uniform/" "/data9/zmen002/kdtree/ss_varden/")

tag=0
k=100
onecore=0
insNum=1
queryType=0 # 001 011 111

resFile="remove_tech.out"
dest=${resFile}
: >${dest}

for solver in ${Solvers[@]}; do
  exe="../build/${solver}"

  for dim in ${Dim[@]}; do
    for dataPath in "${datas[@]}"; do
      for node in ${Node[@]}; do
        files_path="${dataPath}${node}_${dim}"
        echo ">>>>>>>>>>>>>>>>>>>>"

        for ((i = 1; i <= ${insNum}; i++)); do
          echo "${files_path} ${node} ${dim}"
          echo "Threads 192"
          PARLAY_NUM_THREADS=192 ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 3 -q ${queryType}
          echo "Threads 1"
          // PARLAY_NUM_THREADS=1 ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 3 -q ${queryType}

          retval=$?
          if [ ${retval} -eq 124 ]; then
            echo -e "${node}_${dim}.in ${T} -1 -1 -1 -1" >>${dest}
            echo "timeout ${node}_${dim}"
          else
            echo "finish ${node}_${dim}"
          fi
        done
      done
    done
  done
done
