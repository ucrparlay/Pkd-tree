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
benchmarks = ["ss_varden", "uniform", "uniform_bigint"]
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
    solverName = ["test", "cgal", "zdtree", "test_one_core", "zdtree_one_core"]
    resMap = {
        "test": "res_parallel.out",
        "test_one_core": "res_parallel_one_core.out",
        "cgal": "cgal_res_parallel.out",
        "zdtree": "zdtree.out",
        "zdtree_one_core": "zdtree_one_core.out",
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


# * query time by wrap size
Wrap = [
    "2",
    "4",
    "8",
    "16",
    "32",
    "64",
    "128",
    "256",
    "512",
    "1024",
    "2048",
    "4096",
    "8192",
]
if len(sys.argv) > 2 and int(sys.argv[2]) == 1:
    solverName = ["my_kd_wrap", "cgal"]
    resMap = {"my_kd_wrap": "res.out", "cgal": "cgal_res.out"}
    solver = solverName[0]  # * my_kd
    csvWriter = csvSetup("single_wrap")

    node = 1000000
    dim = 5
    for wrap in Wrap:
        P = path + "/" + benchNode + "/500000_5/res_" + wrap + ".out"
        combine(P, csvWriter, solver, benchNode, node, dim)
