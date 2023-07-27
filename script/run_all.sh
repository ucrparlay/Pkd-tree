#!/bin/bash

# ./gen_ss_varden.sh -g 5 -n 8000000 -d 5 -v 1
# ./gen_ss_varden.sh -g 2 -n 50000000 -d 3 -v 1
# ./gen_ss_varden.sh -g 2 -n 100000000 -d 3 -v 1
# ./gen_ss_varden.sh -g 2 -n 500000000 -d 3 -v 1
# ./gen_ss_varden.sh -g 1 -n 1000000000 -d 3 -v 1
./../build/data_generator 1000000 5 5 1
./../build/data_generator 5000000 5 5 1
./../build/data_generator 10000000 5 5 1
./../build/data_generator 50000000 5 5 1
./../build/data_generator 8000000 5 5 1
