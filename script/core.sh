#!/bin/bash
# POSIX

while true; do
    case "$1" in
    --solver)
        shift
        solver=$1
        ;;
    --tag)
        shift
        tag=$1
        ;;
    --dataPath)
        shift
        dataPath=$1
        ;;
    --logPath)
        shift
        logPath=$1
        ;;
    --serial)
        shift
        serial=$1
        ;;
    --node)
        shift
        node=$1
        ;;
    --dim)
        shift
        dim=$1
        ;;
    --insNum)
        shift
        insNum=$1
        ;;
    --)
        shift
        break
        ;;
    esac
    shift
done

# echo "start, solver: ${solver}, tag ${tag}, data path ${dataPath}, log path ${logPath}, running serail: ${serial}"

T=3600
k=100

#* main body
files_path="${dataPath}${node}_${dim}"
log_path="${logPath}${node}_${dim}"
mkdir -p ${log_path}
dest="${log_path}/${resFile}"
: >${dest}
echo ">>>${dest}"

for ((i = 1; i <= ${insNum}; i++)); do
    if [[ ${serial} == 1 ]]; then
        PARLAY_NUM_THREADS=1 numactl -i all ../build/${solver} "${files_path}/${i}.in" ${k} ${tag} >>${dest}
        continue
    fi
    numactl -i all ../build/${solver} "${files_path}/${i}.in" ${k} ${tag} >>${dest}

    retval=$?
    if [ ${retval} -eq 124 ]; then
        echo -e "${node}_${dim}.in ${T} -1 -1 -1 -1" >>${dest}
        echo "timeout ${node}_${dim}"
    else
        echo "finish ${node}_${dim}"
    fi
done
