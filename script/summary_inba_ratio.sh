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
counter=0
while IFS= read -r line; do
    ((counter++))
    if ((counter % 2 == 0)); then
        echo "$line"
    fi
done <"$filename"
