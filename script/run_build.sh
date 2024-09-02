#!/bin/bash
set -o xtrace

# Solvers=("zdtree" "test" "cgal")
Solvers=("test")
Node=(100000000 1000000000)
Tree=(1)
# Tree=(0 1)
Dim=(2 3)
declare -A datas
datas["/localdata/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/localdata/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=0
k=10
insNum=2
summary=0
read_file=0
queryType=$((2#000)) # 1110000
type="build"
round=3
resFile=""

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"

	#* decide output file

	for tree in "${Tree[@]}"; do
		resFile="res_${tree}_${type}.out"

		for dim in "${Dim[@]}"; do
			for dataPath in "${!datas[@]}"; do
				for node in "${Node[@]}"; do
					files_path="${dataPath}${node}_${dim}"
					log_path="${datas[${dataPath}]}${node}_${dim}"
					mkdir -p "${log_path}"
					dest="${log_path}/${resFile}"
					: >"${dest}"
					echo ">>>${dest}"

					for ((i = 1; i <= insNum; i++)); do
						numactl -i all ${exe} -p "${files_path}/${i}.in" -r ${round} -k ${k} -i ${read_file} -s ${summary} -t ${tag} -d ${dim} -q ${queryType} -T ${tree} >>"${dest}"
					done
				done
			done
		done
	done
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
