#!/bin/bash

# ./gen_ss_varden.sh -g 10 -n 1000000 -d 2 -v 1
# ./gen_ss_varden.sh -g 10 -n 5000000 -d 2 -v 1
# ./gen_ss_varden.sh -g 10 -n 8000000 -d 2 -v 1
# ./gen_ss_varden.sh -g 10 -n 10000000 -d 2 -v 1
# ./gen_ss_varden.sh -g 10 -n 50000000 -d 2 -v 1
# ./gen_ss_varden.sh -g 10 -n 100000000 -d 2 -v 1
# ./gen_ss_varden.sh -g 10 -n 500000000 -d 2 -v 1
# N DIM NUM_FILE SERIAL
./../build/data_generator 1000000 5 10 0
./../build/data_generator 5000000 5 10 0
./../build/data_generator 8000000 5 10 0
./../build/data_generator 10000000 5 10 0
./../build/data_generator 50000000 5 10 0
# ./../build/data_generator 10000000 2 2 0
# ./../build/data_generator 50000000 2 2 0
# ./../build/data_generator 100000000 2 2 0
# ./../build/data_generator 500000000 2 2 0
