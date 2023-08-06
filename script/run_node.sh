#!/bin/bash

Solvers=("zdtree" "test")
# Node=(100000000)
Node=(10000000 50000000 100000000 500000000)
declare -A datas
datas["/data9/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data9/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=1
dim=3
k=100
onecore=0
insNum=2

resFile=""

for solver in ${Solvers[@]}; do
    #* decide output file
    if [[ ${solver} == "test" ]]; then
        if [[ ${tag} == 0 ]]; then
            continue
            resFile="res_serial.out"
        else
            if [[ ${onecore} == 0 ]]; then
                resFile="res_parallel.out"
            else
                resFile="res_parallel_one_core.out"
            fi
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
            if [[ ${onecore} == 0 ]]; then
                resFile="zdtree.out"
            else
                resFile="zdtree_one_core.out"
            fi
        fi
    fi

    for dataPath in "${!datas[@]}"; do
        for node in ${Node[@]}; do
            . ./core.sh --solver ${solver} --tag ${tag} --dataPath ${dataPath} --logPath ${datas[${dataPath}]} --serial ${onecore} --node ${node} --dim ${dim} --insNum ${insNum} --core 96 --
        done
    done
done
