#!/bin/bash

folder="KDtree"

while getopts "s:d:f:n:" option; do
    case $option in
    s)
        src=$OPTARG
        ;;
    d)
        dest=$OPTARG
        ;;
    f)
        folder=$OPTARG
        ;;
    n)
        name=$OPTARG
        ;;
    esac
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
    scp -r "zmen002@${src}.cs.ucr.edu:/${srcDisk}/zmen002/kdtree/${name}" "zmen002@${dest}.cs.ucr.edu:/${destDisk}/zmen002/kdtree/${name}"
fi
