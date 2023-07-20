from signal import pause
import subprocess
import os
import sys
from multiprocessing import Pool
from tabnanny import check
from timeit import default_timer as timer
from unittest import case
import math
import csv

oldpath = sys.argv[1]
newpath = sys.argv[2]

lines = open(oldpath, "r").readlines()
f = open(newpath,"w")
firstline = True

for line in lines:

    l = " ".join(line.split())
    l = l.split(" ")
    if firstline==True:
        f.write(' '.join(str(word) for word in l))
        f.write('\n')
        firstline=False
        continue
    f.write(' '.join(str(word) for word in l[1:]))
    f.write('\n')
    
f.close()