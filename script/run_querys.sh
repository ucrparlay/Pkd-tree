#!/bin/bash
set -o xtrace

# Solvers=("zdtree" "test" "cgal")
Solvers=("test")
Node=(100000000 1000000000)
# Tree=(0 1)
Tree=(0)
Dim=(2 3)
declare -A datas
# datas["/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

datas["/localdata/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/localdata/zmen002/kdtree/uniform/"]="../benchmark/uniform/"
tag=0
k=100
insNum=2
queryType=$((2#1101)) # 1110000
type="querys"
resFile=""

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"

	for tree in "${Tree[@]}"; do
		if [[ ${solver} == "test" ]]; then
			resFile="res_${tree}_${type}.out"
		fi

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
						numactl -i all ${exe} -p "${files_path}/${i}.in" -T ${tree} -k ${k} -t ${tag} -d ${dim} -q ${queryType} -i 0 -s 0 -r 3 >>${dest}
					done

				done
			done
		done
	done
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
