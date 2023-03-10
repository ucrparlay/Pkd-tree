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

solverName = ['my_kd', 'cgal']
resMap = {'my_kd': 'res.out', 'cgal': 'cgal_res.out'}

path = "benchmark"
benchDim = "craft_var_dim"
benchNode = "craft_var_node"
Nodes = [10000, 50000, 100000, 500000, 800000, 1000000, 2000000]
Dims = [2, 3, 5, 7, 9, 10, 12, 15]
header = ['solver', 'benchType', 'nodes',
          'dims', 'file', 'buildTime', 'queryTime']

for solver in solverName:
    csvFilePointer = open(path + '/script/' +
                          solver+'.csv', "w", newline='')
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(header)

    node = 100000
    for dim in Dims:
        P = path+'/'+benchDim+'/' + \
            str(node)+"_"+str(dim)+'/'+resMap[solver]
        lines = open(P, "r").readlines()
        for line in lines:
            l = " ".join(line.split())
            l = l.split(' ')
            csvWriter.writerow(
                [solver, benchDim, node, dim, l[0], l[1], l[2]])

    dim = 5
    for node in Nodes:
        P = path+'/'+benchNode+'/' + \
            str(node)+"_"+str(dim)+'/'+resMap[solver]
        lines = open(P, "r").readlines()
        for line in lines:
            l = " ".join(line.split())
            l = l.split(' ')
            csvWriter.writerow(
                [solver, benchNode, node, dim, l[0], l[1], l[2]])
