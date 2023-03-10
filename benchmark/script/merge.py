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

path = "benchmark"
subpath = "craft_var_dim"
Nodes = [100000]
Dims = [2,3,5,7,9,10,12,15]
header = [subpath,'nodes','dims','file','build_time','query_time']
csvFilePointer = open(path+'/' +'craft'+'/'+'summary.csv',"w",newline='')
csvFilePointer.truncate()
csvWriter=csv.writer(csvFilePointer)
csvWriter.writerow(header)

for node in Nodes:
    for dim in Dims:
        P = path+'/'+subpath+'/'+str(node)+"_"+str(dim)+'/res.out'
        lines=open(P,"r").readlines();
        for line in lines:
            l = " ".join(line.split())
            l = l.split(' ')
            csvWriter.writerow([subpath,node,dim,l[0],l[1],l[2]])
            
