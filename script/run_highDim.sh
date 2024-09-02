#!/bin/bash

Solvers=("test")
Node=(100000000)
Dim=(2 3 5 7 9)
# Dim=(7 9)
declare -A datas
datas["/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=0
k=10
onecore=0
insNum=2
queryType=2 # 001 011 111
# queryType=$((2#1100111)) # 1110000
echo $queryType

resFile=""

for solver in ${Solvers[@]}; do
	exe="../build/${solver}"

	#* decide output file
	if [[ ${solver} == "test" ]]; then
		resFile="res_highDim.out"
	elif [[ ${solver} == "cgal" ]]; then
		resFile="cgal_highDim.out"
	fi

	for dim in ${Dim[@]}; do
		for dataPath in "${!datas[@]}"; do
			for node in ${Node[@]}; do
				files_path="${dataPath}${node}_${dim}"
				log_path="${datas[${dataPath}]}${node}_${dim}"
				mkdir -p ${log_path}
				dest="${log_path}/${resFile}"
				: >${dest}
				echo ">>>${dest}"

				for ((i = 1; i <= ${insNum}; i++)); do
					PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>${dest}
				done
			done
		done
	done
done
