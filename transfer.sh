#!/bin/bash

folder="KDtree"

while true; do
    case "$1" in
    --s)
        shift
        src=$1
        ;;
    --d)
        shift
        dest=$1
        ;;
    --folder)
        shift
        folder=$1
        ;;
    --name)
        shift
        name=$1
        ;;
    --)
        shift
        break
        ;;
    esac
    shift
done

if [[ ${folder} == "KDtree" ]]; then 
    scp -r "zmen002@${src}.cs.ucr.edu:~/KDtree/${name}" "zmen002@${dest}.cs.ucr.edu:~/KDtree/"
else 
    if [[ ${src} == "cheetah" ]]; then
        srcDisk="ssd0"
        destDisk="data9"
    else 
        srcDisk="data9"
        destDisk="ssd0"
    fi
    scp -r "zmen002@${src}.cs.ucr.edu:/${srcDisk}/zmen002/kdtree/${name}" "zmen002@${dest}.cs.ucr.edu:/${destDisk}/zmen002/kdtree/"
fi