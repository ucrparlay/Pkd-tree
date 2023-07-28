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

path = "../benchmark"
benchmarks = ["ss_varden", "uniform"]
storePrefix = "data/"
Nodes = [10000000, 50000000, 100000000, 500000000]
Dims = [2, 3, 5, 7, 9]
header = [
    "solver",
    "benchType",
    "nodes",
    "dims",
    "file",
    "buildTime",
    "queryTime",
    "aveDeep",
    "aveQueryVisNodeNum",
]


def combine(P, csvWriter, solver, benchName, node, dim):
    if not os.path.isfile(P):
        print("No file fonund: "+ P)
        return
    lines = open(P, "r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(" ")
        while len(l)<5 :
            l.append("-1")
        csvWriter.writerow([solver, benchName, node, dim, l[0], l[1], l[2], l[3], l[4]])


def csvSetup(solver):
    csvFilePointer = open(storePrefix + solver + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(header)
    return csvWriter


# * merge the result
if len(sys.argv) > 1 and int(sys.argv[1]) == 1:
    solverName = ["test", "cgal", "zdtree"]
    resMap = {
        "test": "res_parallel.out",
        # "test_one_core": "res_parallel_one_core.out",
        "cgal": "cgal_res_parallel.out",
        "zdtree": "zdtree.out"
        # "zdtree_one_core": "zdtree_one_core.out",
    }

    for solver in solverName:
        csvWriter = csvSetup(solver)

        dim = 3
        for bench in benchmarks:
            for node in Nodes:
                P = (
                    path
                    + "/"
                    + bench
                    + "/"
                    + str(node)
                    + "_"
                    + str(dim)
                    + "/"
                    + resMap[solver]
                )
                combine(P, csvWriter, solver, bench, node, dim)


cores = [1, 8, 16, 24, 48, 96]

if len(sys.argv) > 1 and int(sys.argv[1]) == 2:
    solverName = ["test", "zdtree"]
    resMap = {
        "test": "res_parallel.out",
        "zdtree": "zdtree.out"
    }

    for solver in solverName:
        csvFilePointer = open(storePrefix + solver + "_scala.csv", "w", newline="")
        csvFilePointer.truncate()
        csvWriter = csv.writer(csvFilePointer)
        csvWriter.writerow(['solver', 'benchType', 'nodes', 'dims', 'file', 'buildTime', 'core'])

        dim = 3
        for bench in benchmarks:
            for node in Nodes:
                for core in cores:
                  P = (
                      path
                      + "/"
                      + bench
                      + "/scalability/"
                      + str(node)
                      + "_"
                      + str(dim)
                      + "/"
                      + str(core)
                      + '_'
                      + resMap[solver]
                  )
                  if not os.path.isfile(P):
                      print("No file fonund: "+ P)
                      continue
                  lines = open(P, "r").readlines()
                  for line in lines:
                      l = " ".join(line.split())
                      l = l.split(" ")
                      while len(l)<4 :
                          l.append("-1")
                      csvWriter.writerow([solver, bench, node, dim, l[0], l[1],str(core)])
                      csvWriter.writerow([solver, bench, node, dim, l[2], l[3],str(core)])
