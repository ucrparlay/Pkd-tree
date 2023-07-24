#!/bin/bash

Solvers=("test")
Cores=(8 24 48 96)
# Node=(100000)
Node=(1000000000)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/scalability/"

Tag=(0 1)
dim=3
k=100
onecore=0
insNum=1

resFile=""

for solver in ${Solvers[@]}; do
    for tag in ${Tag[@]}; do
        #* decide output file
        if [[ ${solver} == "test" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="res_serial.out"
            else
                resFile="res_parallel.out"
                # resFile="res_parallel_one_core.out"
            fi
        elif [[ ${solver} == "cgal" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
                resFile="cgal_res_serial.out"
            else
                resFile="cgal_res_parallel.out"
            fi
        elif [[ ${solver} == "zdtree" ]]; then
            if [[ ${tag} == 0 ]]; then
                continue
            else
                resFile="zdtree.out"
            fi
        fi

        for dataPath in "${!datas[@]}"; do
            for node in ${Node[@]}; do
                for core in ${Cores[@]}; do
                    . ./core.sh --solver ${solver} --tag ${tag} --dataPath ${dataPath} --logPath ${datas[${dataPath}]} --serial ${onecore} --node ${node} --dim ${dim} --insNum ${insNum} --core ${core} --
                done
            done
        done
    done
done
