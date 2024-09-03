#!/bin/bash

# Function to kill process tree
kill_tree() {
	local pid=$1
	local and_children=$2

	if [ -z "$pid" ]; then
		echo "No PID provided."
		return 1
	fi

	# Get children PIDs
	children=$(pgrep -P "$pid")

	# Kill children
	for child_pid in $children; do
		kill_tree "$child_pid" true
	done

	if [ "$and_children" = true ]; then
		# Kill parent process
		kill -9 "$pid"
	fi
}

# Check if PID is provided
if [ -z "$1" ]; then
	echo "Usage: $0 <pid>"
	exit 1
fi

# Get PID
pid="$1"

# Kill process tree
kill_tree "$pid" true
