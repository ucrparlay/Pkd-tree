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

print(os.getcwd())


def combine(P, csvWriter, solver, benchName, node, dim):
    lines = open(P, "r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(' ')
        if solver == 'cgal':
            csvWriter.writerow(
                [solver, benchName, node, dim, l[0], l[1], l[2], -1])
        else:
            csvWriter.writerow(
                [solver, benchName, node, dim, l[0], l[1], l[2], l[3]])

def check(P):
    lines = open(P,"r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(' ')
        print(l[-1])
        if l[-1]!='ok':
            print("wrong",P);

path = "../benchmark"
benchDim = "craft_var_dim"
benchNode = "craft_var_node"
Nodes = [10000, 50000, 100000, 500000, 800000, 1000000, 2000000]
Dims = [2, 3, 5, 7, 9, 10, 12, 15]
header = ['solver', 'benchType', 'nodes',
          'dims', 'file', 'buildTime', 'queryTime', 'wrapSize']

#* merge the result
if (len(sys.argv) > 1 and int(sys.argv[1]) == 1):
    solverName = ['my_kd', 'cgal']
    resMap = {'my_kd': 'res.out', 'cgal': 'cgal_res.out'}

    for solver in solverName:
        csvFilePointer = open(solver+'.csv', "w", newline='')
        csvFilePointer.truncate()
        csvWriter = csv.writer(csvFilePointer)
        csvWriter.writerow(header)

        node = 100000
        for dim in Dims:
            P = path+'/'+benchDim+'/' + \
                str(node)+"_"+str(dim)+'/'+resMap[solver]
            combine(P, csvWriter, solver, benchDim, node, dim)

        dim = 5
        for node in Nodes:
            P = path+'/'+benchNode+'/' + \
                str(node)+"_"+str(dim)+'/'+resMap[solver]
            combine(P, csvWriter, solver, benchNode, node, dim)

#* get the value
Wrap = ["2", "4", "8", "16", "32", "64", "128", "256",
        "512", "1024", "2048", "4096", "8192", "16384"]
if (len(sys.argv) > 2 and int(sys.argv[2]) == 1):
    solverName = ['my_kd_wrap', 'cgal']
    resMap = {'my_kd_wrap': 'res.out', 'cgal': 'cgal_res.out'}

    solver = solverName[0]  # * my_kd
    csvFilePointer = open('single_wrap.csv', "w", newline='')
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(header)

    node = 1000000
    dim = 5
    for wrap in Wrap:
        P = path+'/'+benchNode+"/1000000_5/res_"+wrap+'.out'
        combine(P, csvWriter, solver, benchNode, node, dim)

#* check the correctness
if (len(sys.argv) > 3 and int(sys.argv[3]) == 1):
    
    node = 100000
    for dim in Dims:
        P = path+'/'+benchDim+'/' + \
            str(node)+"_"+str(dim)+'/'+'Correct.out'
        check(P)

    dim = 5
    for node in Nodes:
        P = path+'/'+benchNode+'/' + \
            str(node)+"_"+str(dim)+'/'+'Correct.out'
        check(P)
