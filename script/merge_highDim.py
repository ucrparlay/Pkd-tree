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
# benchmarks = ["ss_varden"]
benchmarks = ["ss_varden", "uniform"]
storePrefix = "data/"
Nodes = [100000000]
Dims = [2, 3, 5, 7, 9]

solverName = ["test", "cgal", "BhlTree", "LogTree"]
resMap = {
    "test": "res_highDim.out",
    "cgal": "cgal_highDim.out",
    "BhlTree": "BhlTree_highDim.out",
    "LogTree": "LogTree_highDim.out",
    # "zdtree": "zdtree_highDim.out",
}

common = [
    "solver",
    "benchType",
    "nodes",
    "dims",
]

#! order by test order
files = ["highDim"]

build_header = [
    "build",
    "aveDepth",
    "k=10",
    "depth",
    "visNum",
]
file_header = {
    "highDim": build_header,
}

prefix = [0] * len(files)

# TODO change order


def combine(P, file, csvWriter, solver, benchName, node, dim):
    if not os.path.isfile(P):
        print("No file fonund: " + P)
        return

    lines = open(P, "r").readlines()
    sep_lines = []
    for line in lines:
        l = " ".join(line.split())
        l = l.split(" ")
        sep_lines.append(l)

    width = len(file_header[file])
    l = prefix[files.index(file)]
    r = l + width
    num = 2 if solverName.index(solver)<=1 else 1
    for i in range(0, len(sep_lines), num):
        line = [0] * width
        for j in range(i, num):
            for k in range(l, r):
                line[k - l] = line[k - l] + float(sep_lines[j][k]) / num

        csvWriter.writerow(
            [solver, benchName, node, dim] + list(map(lambda x: round(x, 3), line))
        )


def csvSetup(solver):
    csvFilePointer = open(storePrefix + solver + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(common + file_header[file])
    return csvWriter


def calculatePrefix():
    for i in range(0, len(files), 1):
        prefix[i] = len(file_header[files[i]])
    l = prefix[0]
    prefix[0] = 1
    for i in range(1, len(files), 1):
        r = prefix[i]
        prefix[i] = l + prefix[i - 1]
        l = r

    print(prefix)


# * merge the result
if len(sys.argv) > 1 and int(sys.argv[1]) == 1:
    calculatePrefix()
    for file in files:
        csvWriter = csvSetup(file)

        for dim in Dims:
            for bench in benchmarks:
                for solver in solverName:
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
                        combine(P, file, csvWriter, solver, bench, node, dim)
