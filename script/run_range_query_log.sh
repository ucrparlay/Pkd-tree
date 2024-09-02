#!/bin/bash
set -o xtrace

Solvers=("test" "cgal")
# Solvers=("test")
Node=(1000000000)
Dim=(3)
declare -A datas
datas["/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=0
k=10
insNum=1
queryType=$((2#1000)) # 1110000
type="range_query_log"
resFile=""

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"

	#* decide output file
	if [[ ${solver} == "test" ]]; then
		resFile="res_${type}.out"
	elif [[ ${solver} == "cgal" ]]; then
		resFile="cgal_${type}.out"
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

					# NOTE: main part
					PARLAY_NUM_THREADS=192 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -i 0 >>${dest}
					# NOTE:end

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

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
