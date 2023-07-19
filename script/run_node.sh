#!/bin/bash

Solvers=("cgal")
# Node=(100000)
Node=(10000000 50000000 100000000 500000000 1000000000)
declare -A datas
# datas["/ssd0/zmen002/kdtree/uniform/"]="../benchmark/uniform/"
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"

Tag=(0 1)
dim=3
k=100
onecore=0
insNum=2

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
                . ./core.sh --solver ${solver} --tag ${tag} --dataPath ${dataPath} --logPath ${datas[${dataPath}]} --serial ${onecore} --node ${node} --dim ${dim} --insNum ${insNum} --
            done
        done
    done
done
