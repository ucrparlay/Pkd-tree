#!/bin/bash

set -x

#perfcmd="perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses"
events="cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses"
Solvers=("bhltree" "logtree")
declare -A datasets
#datasets=("CHEM" "Cosmo50" "GeoLifeNoScale" "Household" "HT" "OpenStreetMap" "osm")

datasets["CHEM"]=16
datasets["Cosmo50"]=3
datasets["GeoLifeNoScale"]=3
datasets["Household"]=7
datasets["HT"]=10
datasets["osm"]=2

#datasets["OpenStreetMap"]=2

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

		perfdir="/localdata/zshen055/perf/kdtree/${ds}"
		mkdir -p ${perfdir}

		perf_data_name="${perfdir}/${solver}.perf.data"
		perf_report_name="${perfdir}/${solver}.perf.report"

		# NOTE: warm-up here
		logname="${solver}.out"
		${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 4 -r 1 > ${logdir}/${logname} 2>&1

		logname="${solver}.perf.out"
		#func_name="serialrangequery"
		func_name="orthogonalQuery"

		ctl_dir=/tmp/

		ctl_fifo=${ctl_dir}perf_ctl.fifo
		test -p ${ctl_fifo} && unlink ${ctl_fifo}
		mkfifo ${ctl_fifo}
		exec {ctl_fd}<>${ctl_fifo}

		ctl_ack_fifo=${ctl_dir}perf_ctl_ack.fifo
		test -p ${ctl_ack_fifo} && unlink ${ctl_ack_fifo}
		mkfifo ${ctl_ack_fifo}
		exec {ctl_fd_ack}<>${ctl_ack_fifo}

		perf record -o ${perf_data_name} -e ${events} -D -1 --control fd:${ctl_fd},${ctl_fd_ack} ${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 4 -r 1 -pcf ${ctl_fd} -pcaf ${ctl_fd_ack} > ${logdir}/${logname} 2>&1

		exec {ctl_fd_ack}>&-
		unlink ${ctl_ack_fifo}

		exec {ctl_fd}>&-
		unlink ${ctl_fifo}

		#perf report --stdio --input=${perf_data_name} >${perf_report_name}
		#grep -E "Samples|Event count|\\b${func_name}\\b" ${perf_report_name} | awk '$1!="0.00%"' | awk '/Event count/ {print $NF} /'"${func_name}"'/ {gsub("%", "", $1); print $1}' | awk '{v[NR]=$1;} END {for(i=1;i<=NR;i+=2){if(i+1<=NR){res=(v[i]*v[i+1])/100; printf "%d\n", res;}}}' > ${perfdir}/${solver}.perf.res
		cp ${perf_report_name} ${logdir}
		grep 'of event' ${perf_report_name} | awk '{print $NF}' > ${logdir}/${solver}.perf.res
		grep 'Event count' ${perf_report_name} | awk '{print $NF}' >> ${logdir}/${solver}.perf.res


		#logname="${solver}.perf.ext.all"
		#$perfcmd ${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 4 -r 1 > ${logdir}/${logname} 2>&1

		#logname="${solver}.perf.ext.noquery"
		#$perfcmd ${exe} -p ${basepath}/${ds}.in -k 10 -t 65536 -d ${dim} -q 65536 -r 1 > ${logdir}/${logname} 2>&1
	done
done
