#!/bin/bash

set -x

path_u=/data3/zmen002/kdtree/uniform/1000000000_3/1.in
path_v=/data3/zmen002/kdtree/ss_varden/1000000000_3/1.in

logdir=../benchmark/ood

for ptype in 1 2; do
# del_mode=0, ins_mode=0, build_mode=1 (build only), seg_mode=0 (unused)
# downsize_k=10, ins_ratio=0, tag=0
tag=$((2560+65536))
k=100
#numactl -i all ../build/test_pg -T $ptype -p $path_u -pa $path_v -k $k -t $tag -d 3 -q 1 -r 4 > $logdir/$ptype.uv.out 
## NOTE reduce k to 10
k=1
numactl -i all ../build/test_pg -T $ptype -p $path_v -pa $path_u -k $k -t $tag -d 3 -q 1 -r 1 > $logdir/$ptype.vu.out
done
