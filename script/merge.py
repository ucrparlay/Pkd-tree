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

solverName = ['my_kd']
resMap = {'my_kd': 'res.out', 'cgal': 'cgal_res.out'}
print(os.getcwd())

def combine(P, csvWriter,solver,benchName, node, dim):
    lines = open(P, "r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(' ')
        csvWriter.writerow(
            [solver, benchName, node, dim, l[0], l[1], l[2], l[3]])

path = "../benchmark"
benchDim = "craft_var_dim"
benchNode = "craft_var_node"
Nodes = [10000, 50000, 100000, 500000, 800000, 1000000, 2000000]
Dims = [2, 3, 5, 7, 9, 10, 12, 15]
header = ['solver', 'benchType', 'nodes',
          'dims', 'file', 'buildTime', 'queryTime','wrapSize']

if(len(sys.argv)>1 and int(sys.argv[1])==1):
    for solver in solverName:
        csvFilePointer = open(solver+'.csv', "w", newline='')
        csvFilePointer.truncate()
        csvWriter = csv.writer(csvFilePointer)
        csvWriter.writerow(header)

        node = 100000
        for dim in Dims:
            P = path+'/'+benchDim+'/' + \
                str(node)+"_"+str(dim)+'/'+resMap[solver]
            combine(P,csvWriter,solver,benchDim,node,dim)

        dim = 5
        for node in Nodes:
            P = path+'/'+benchNode+'/' + \
                str(node)+"_"+str(dim)+'/'+resMap[solver]
            combine(P,csvWriter,solver,benchNode,node,dim)

Wrap=["2","4","8","16","32","64","128","256"]
if(len(sys.argv)>2 and int(sys.argv[2])==1):
    solver = solverName[0] #* my_kd
    csvFilePointer = open(solver+'_wrap.csv', "w", newline='')
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(header)
    
    node = 1000000
    dim = 5
    for wrap in Wrap:
        P=path+'/'+benchNode+"/1000000_5/res_"+wrap+'.out'
        combine(P,csvWriter,solver,benchNode,node,dim)
