#!/bin/bash
# {
# 	sleep 10s
# 	# kill $$
# 	pkill -P $$
# } &
set -o xtrace
# Solvers=("rtree" "test" "cgal")
Solvers=("test")
# Solvers=("zdtree" "test")
# Node=(100000000 1000000000)
Node=(1000000000)
# Dim=(2 3 5 9)
Dim=(2)
# Dim=(2 9)
declare -A datas
datas["/data/legacy/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data/legacy/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=2
k=10
insNum=1
# queryType=$((2#1)) # 1110000
queryType=$((2#1001)) # 1110000
echo $queryType
type="summary"
rounds=3

resFile=""

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"

	#* decide output file
	if [[ ${solver} == "test" ]]; then
		# resFile="res_${type}.out"
		resFile="res_${type}_once.out"
	elif [[ ${solver} == "rtree" ]]; then
		resFile="rtree_${type}_once.out"
	elif [[ ${solver} == "cgal" ]]; then
		resFile="cgal_${type}_once.out"
	elif [[ ${solver} == "zdtree" ]]; then
		resFile="zdtree_${type}.out"
		exe="/home/zmen002/pbbsbench_x/build/zdtree"
	fi

	for dim in "${Dim[@]}"; do
		if [ "${dim}" -gt 3 ] && [ "${solver}" == "zdtree" ]; then
			continue
		fi

		for dataPath in "${!datas[@]}"; do
			for node in "${Node[@]}"; do
				files_path="${dataPath}${node}_${dim}"
				log_path="${datas[${dataPath}]}${node}_${dim}"
				mkdir -p "${log_path}"
				dest="${log_path}/${resFile}"
				: >"${dest}"
				echo ">>>${dest}"

				for ((i = 1; i <= insNum; i++)); do

					export PARLAY_NUM_THREADS=192
					# export TEST_CGAL_THREADS=192
					numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -r ${rounds} -i 1 -s 1 >>"${dest}"

					retval=$?
					if [ ${retval} -eq 124 ]; then
						echo -e "timeout" >>"${dest}"
						echo "timeout ${node}_${dim}"
					else
						echo "finish ${node}_${dim}"
					fi
				done
			done
		done
	done
done
