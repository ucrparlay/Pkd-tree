#!/bin/bash

set -x

# Solvers=("zdtree" "test")
Solvers=("bhltree" "logtree")
#Solvers=("logtree")
#Node=(500000000 1000000000)
Node=(10000000 20000000 50000000 100000000 200000000 500000000 1000000000)
#Node=(10000000)
#Dim=(2 3)
#Dim=(2 3 5 7 9)
#Dim=(2 3 5 9)
Dim=(3)
declare -A datas
datas["/data/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=2
k=100
onecore=0
#insNum=2
insNum=1
queryType=3 # 001 011 111

resFile=""

for solver in ${Solvers[@]}; do
	exe="../build/${solver}"

	#* decide output file
	if [[ ${solver} == "test" ]]; then
		resFile="res.out"
	elif [[ ${solver} == "bhltree" ]]; then
		resFile="bhltree.out"
		exe="../build/test_pg -T 1"
	elif [[ ${solver} == "logtree" ]]; then
		resFile="logtree.out"
		exe="../build/test_pg -T 2"
	elif [[ ${solver} == "cgal" ]]; then
		resFile="cgal.out"
	elif [[ ${solver} == "zdtree" ]]; then
		resFile="zdtree.out"
		exe="/home/zmen002/pbbsbench_x/build/zdtree"
	elif [[ ${solver} == "cpam" ]]; then
		resFile="cpam.out"
		exe="/home/zmen002/CPAM_x/build/cpam_query"
	fi

	for dim in "${Dim[@]}"; do
		for dataPath in "${!datas[@]}"; do
			for node in ${Node[@]}; do
				files_path="${dataPath}${node}_${dim}"
				log_path="${datas[${dataPath}]}${node}_${dim}"
				mkdir -p ${log_path}
				dest="${log_path}/${resFile}"
				: >${dest}
				echo ">>>${dest}"

				for ((i = 1; i <= ${insNum}; i++)); do
					# del_mode=0 (unused), ins_mode=0 (unused), build_mode=1, seg_mode=0 (unused)
					# downsize_k=0 (unused), ins_ratio=0 (unused), tag=0
					#queryType=5 # k-NN and range query
					queryType=0 # no query
					k=10        # unused
					echo "Test for memory usage and cache misses"
					tag=65536
					$(which time) -v ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r 1 >>${dest}.mem.all 2>&1
					#perf stat -e cache-misses ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r 1 >>${dest}.cache.all 2>&1
					# $(which time) -v ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r 1 >>${dest}.mem.prep 2>&1
					#perf stat -e cache-misses ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r 1 >>${dest}.cache.prep 2>&1

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
