path="../benchmark/craft_var_node/"
node="1000000"
Wrap=("16" "32" "64" "128" "256")
Dims="5"
tester="test"
resFile="res_"
T="1200"

for wrap in ${Wrap[@]}
do
    files_path="${path}${node}_${Dims}"
    : > "${files_path}/${resFile}${wrap}.out"
    echo "-------${files_path}_${wrap}"

    for file in "${files_path}/"*.in
    do
        file_name="${file##*"/"}"
        timeout ${T} ../build/${tester} ${file} ${wrap} >> "${files_path}/${resFile}${wrap}.out"
        retval=$?
        if [ ${retval} -eq 124 ]
        then
            echo -e "${file_name} -1 -1" >> "${files_path}/${resFile}"
            echo "timeout ${file_name}"
        else
            echo "finish ${file_name}"
        fi
    done
done
