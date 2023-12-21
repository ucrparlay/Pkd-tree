#!/bin/bash

# Check if a filename is provided as an argument
if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi

filename=$1

# Check if the file exists
if [ ! -e "$filename" ]; then
    echo "File not found: $filename"
    exit 1
fi

# Print every 2nd, 4th, 6th line, and so on
echo "ratio build1	maxdepth1	averagedepth1	1nn1	visnodeNum1	5nn1	visnodeNum1	100nn1	visnodeNum1	build2	maxdepth2	averagedepth2	1nn2	visnodeNum2	5nn2	visnodeNum2	100nn2	visnodeNum2	build3	maxdepth3	averagedepth3	1nn3	visnodeNum3	5nn3	visnodeNum3	100nn3	visnodeNum3"
counter=0
cnt=0
ratios=(0 1 2 3 4 5 6 10 20 30 40 45 46 47 48 49 50)
while IFS= read -r line; do
    ((counter++))
    if ((counter % 2 == 0)); then
        echo -n "${ratios[${cnt}]} "
        echo "$line"
        ((cnt++))
    fi
done <"$filename"
