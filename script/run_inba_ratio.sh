#!/bin/bash
# {
#     sleep 210m
#     kill $$
# } &
set -o xtrace
Solvers=("test")
Node=(1000000000)
# Inba=(1 2 3 4 5 6 10 20 30 40 45 46 47 48 49 50)
Inba=(3 4 5 6 10 20 30 40 45 46 47 48 49 50)
Dim=(3)
declare -A datas
# datas["/ssd0/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"

inbaQuery=1
tag=0
k=10
queryType=1024 # 001 011 111
echo $queryType

if [[ ${inbaQuery} -eq 0 ]]; then
	resFile="inba_ratio_knn.out"
else
	resFile="inba_ratio_rc.out"
fi

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"

	for dim in "${Dim[@]}"; do
		for dataPath in "${!datas[@]}"; do
			for node in "${Node[@]}"; do
				files_path="${dataPath}${node}_${dim}"
				dest="data/${resFile}"
				# : >${dest}
				echo ">>>${dest}"

				# NOTE: run basic first
				# PARLAY_NUM_THREADS=192 INBA_QUERY=${inbaQuery} INBA_BUILD=0 numactl -i all ${exe} -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>"${dest}"

				# NOTE: run others then
				for ratio in "${Inba[@]}"; do
					PARLAY_NUM_THREADS=192 INBALANCE_RATIO=${ratio} INBA_QUERY=${inbaQuery} INBA_BUILD=1 numactl -i all ${exe} -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>"${dest}"
				done
			done
		done
	done
done
