#!/bin/bash

./transfer -s cheetah -d dynamic -f data -n ss_varden/1000000000_3
./transfer -s cheetah -d dynamic -f data -n uniform/1000000000_2
./transfer -s cheetah -d dynamic -f data -n uniform/1000000000_3

./transfer -s cheetah -d dynamic -f data -n ss_varden/100000000_5
./transfer -s cheetah -d dynamic -f data -n ss_varden/100000000_7
./transfer -s cheetah -d dynamic -f data -n ss_varden/100000000_9

./transfer -s cheetah -d dynamic -f data -n uniform/100000000_5
./transfer -s cheetah -d dynamic -f data -n uniform/100000000_7
./transfer -s cheetah -d dynamic -f data -n uniform/100000000_9
