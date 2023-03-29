#!/bin/bash
wget -O ../src_x/DBSCAN.zip  https://www.dropbox.com/s/xtf3134zcq08rt9/DBSCAN_v2.0_ubuntu14.04_bin.zip?dl=1
unzip ../src_x/DBSCAN.zip -d ../src_x/
rm ../src_x/DBSCAN.zip

gnum=$1
node=$2
Dims=("2" "3" "5" "7" "9")
varDensity=$3
vardenPath="../src_x/DBSCAN"
outPath="../benchmark/ss_varden/"

for dim in ${Dims[@]}
do
    mkdir ${outPath}${node}_${dim}
    for gi in $(seq 1 1 ${gnum})
    do
        sleep 2
        ./${vardenPath} -algo 0 -ds ${outPath}${node}_${dim}/${gi}.in -n ${node} -d ${dim} -vd ${varDensity}
    done
done