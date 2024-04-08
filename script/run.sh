#!/bin/bash

{
	sleep 6h
	sleep 45m
	kill $$
} &

./run_inba_ratio.sh
./scalability.sh
