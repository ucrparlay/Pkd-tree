#!/bin/bash
#
echo "build, depth, k=1, depth, visNum, k=10, depth, visNum, k=100, depth, visNum ">"data/real_world"
sed 's/ /,/g' "../benchmark/real_world/res_real_world.out" >> "data/real_world.csv"
sed 's/ /,/g' "../benchmark/real_world/zdtree_real_world.out" >> "data/real_world.csv"
sed 's/ /,/g' "../benchmark/real_world/cgal_real_world.out" >> "data/real_world.csv"
