#!/bin/bash
#* source: https://sites.google.com/view/approxdbscan

download=0

while getopts "w:g:n:d:v:" option; do
    case $option in
    w)
        download=$OPTARG
        ;;
    g)
        gnum=$OPTARG
        ;;
    n)
        node=$OPTARG
        ;;
    d)
        dim=$OPTARG
        ;;
    v)
        varDensity=$OPTARG
        ;;
    esac
done

if [[ ${download} -eq 1 ]]; then
    wget -O ../src_x/DBSCAN.zip https://www.dropbox.com/s/xtf3134zcq08rt9/DBSCAN_v2.0_ubuntu14.04_bin.zip?dl=1
    unzip ../src_x/DBSCAN.zip -d ../src_x/
    rm ../src_x/DBSCAN.zip
fi

echo "${download} ${gnum} ${node} ${dim} ${varDensity}"

vardenPath="../src_x/DBSCAN"
outPath="/data9/zmen002/kdtree/ss_varden/"

mkdir ${outPath}${node}_${dim}

for gi in $(seq 1 1 ${gnum}); do
    oldPath="${outPath}${node}_${dim}/${gi}.in"
    newPath="${outPath}${node}_${dim}/$new{gi}.in"

    sleep 2
    ./${vardenPath} -algo 0 -ds ${oldPath} -n ${node} -d ${dim} -vd ${varDensity}

    # while IFS= read -r line; do
    #     echo ${line}
    # done <${oldPath}
    # head -10 ${oldPath}
    python3.9 wash_varden.py ${oldPath} ${newPath}

    rm ${oldPath}
    mv ${newPath} ${oldPath}
done
