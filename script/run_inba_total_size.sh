#!/bin/bash
# {
#     sleep 210m
#     kill $$
# } &
set -o xtrace
Solvers=("test")
Node=(1000000000)
Inba=(1 2 5 10 20 30 40 45 48 49 50)
# Inba=(30 50)
# Inba=(10 20 30 40 45 48 49 50 5 2 1)
Dim=(3)
declare -A datas
# datas["/ssd0/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data/zmen002/kdtree/uniform/"]="../benchmark/uniform/"
datas["/data/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
# datas["/data/legacy/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

inbaQuery=2
tag=0
k=10
queryType=1024 # 001 011 111
echo $queryType

if [[ ${inbaQuery} -eq 0 ]]; then
	resFile="inba_ratio_knn.out"
elif [[ ${inbaQuery} -eq 1 ]]; then
	resFile="inba_ratio_rc.out"
elif [[ ${inbaQuery} -eq 2 ]]; then
	resFile="inba_ratio_perf_total_size.out"
elif [[ ${inbaQuery} -eq 3 ]]; then
	resFile="inba_ratio_perf_incre.out"
fi

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"
	dest="data/${resFile}"
	: >"${dest}"

	for dim in "${Dim[@]}"; do
		for dataPath in "${!datas[@]}"; do
			for node in "${Node[@]}"; do
				files_path="${dataPath}${node}_${dim}"
				echo ">>>${dest}"

				# NOTE: run others then
				for ratio in "${Inba[@]}"; do
					# export PARLAY_NUM_THREADS=1
					export INBALANCE_RATIO=$ratio
					# export INBA_QUERY=$inbaQuery # WARN: may change in incre
					export INBA_BUILD=1
					command="numactl -i all ${exe} -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -s 0 -r 1 -i 1"
					output=$($command)
					echo "$output" >>"$dest"
				done
			done
		done
	done
done

#!/bin/bash
