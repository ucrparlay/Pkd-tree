#!/bin/bash

set -x

perfcmd="perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses"
Solvers=("bhltree" "logtree")
declare -A datasets
#datasets=("CHEM" "Cosmo50" "GeoLifeNoScale" "Household" "HT" "OpenStreetMap" "osm")
datasets["CHEM"]=16
datasets["Cosmo50"]=3
datasets["GeoLifeNoScale"]=3
datasets["Household"]=7
datasets["HT"]=10
#datasets["OpenStreetMap"]=2
datasets["osm"]=2
basepath="/data3/zmen002/kdtree/geometry"

for solver in ${Solvers[@]}; do
	#* decide output file
	if [[ ${solver} == "bhltree" ]]; then
		exe="../build/test_pg -T 1"
	elif [[ ${solver} == "logtree" ]]; then
		exe="../build/test_pg -T 2"
	fi

	for ds in ${!datasets[@]}; do
		logdir="../benchmark/real_world/${ds}"
		mkdir -p ${logdir}
		dim=${datasets[${ds}]}
		$perfcmd ${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 4 -r 1

		#logname="${solver}.perf.ext.all"
		#$perfcmd ${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 4 -r 1 > ${logdir}/${logname} 2>&1

		logname="${solver}.perf.ext.noquery"
		$perfcmd ${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 0 -r 1 > ${logdir}/${logname} 2>&1
	done
done
