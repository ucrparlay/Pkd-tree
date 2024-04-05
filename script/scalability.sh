#!/bin/bash
#
# set -o xtrace

# Solvers=("test")
Solvers=("test")
Node=(1000000000)
declare -A datas
datas["/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/scalability/"
datas["/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/scalability/"

# declare -a cores=(1)
declare -a cores=(1 2 4 8 16 24 48 96 192)

declare -a threads=(
	"PARLAY_NUM_THREADS=1"
	"PARLAY_NUM_THREADS=2"
	"PARLAY_NUM_THREADS=4"
	"PARLAY_NUM_THREADS=8"
	"PARLAY_NUM_THREADS=16"
	"PARLAY_NUM_THREADS=24"
	"PARLAY_NUM_THREADS=48"
	"PARLAY_NUM_THREADS=96"
	"PARLAY_NUM_THREADS=192"
)

declare -a commands=(
	"taskset -c 0-3:4"                 # 1 thread
	"taskset -c 0-7:4 numactl -i all"  # 2 threads
	"taskset -c 0-15:4 numactl -i all" # 4 threads
	"taskset -c 0-31:4 numactl -i all" # 8 threads
	"taskset -c 0-63:4 numactl -i all" # 16 threads
	"taskset -c 0-95:4 numactl -i all" # 24 threads
	"taskset -c 0-95:2 numactl -i all" # 48 threads
	"taskset -c 0-95 numactl -i all"   # 96 threads
	"numactl -i all"                   # 192 threads
)

tag=2
dim=3
k=10

resFile=""

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"
	#* decide output file
	if [[ ${solver} == "test" ]]; then
		resFile="res.out"
	elif [[ ${solver} == "cgal" ]]; then
		resFile="cgal.out"
	elif [[ ${solver} == "zdtree" ]]; then
		resFile="zdtree.out"
		exe="/home/zmen002/pbbsbench_x/build/zdtree"
	fi

	for i in "${!threads[@]}"; do
		for dataPath in "${!datas[@]}"; do
			for node in "${Node[@]}"; do
				logPath=${datas[${dataPath}]}

				files_path="${dataPath}${node}_${dim}"
				log_path="${logPath}${node}_${dim}"
				mkdir -p "${log_path}"
				dest="${log_path}/${cores[${i}]}_${resFile}"
				: >"${dest}"
				echo ">>>${dest}"

				export "${threads[${i}]}"
				${commands[${i}]} "${exe}" -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -r 1 -q 0 -i 1 >>"${dest}"

			done
		done
	done
done
