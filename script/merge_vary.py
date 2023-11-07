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
Nodes = [10000000, 50000000, 100000000, 500000000]
# Dims = [2, 3]
Dims = [3]

solverName = ["test", "zdtree"]
resMap = {"test": "res.out", "cgal": "cgal.out", "zdtree": "zdtree.out"}

common = [
    "solver",
    "benchType",
    "nodes",
    "dims",
]

#! order by test order
files = [
    "build",
    "knn",
    "count",
    "rquery",
    "insert",
    "delete",
]

build_header = ["build"]
insert_header = ["10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"]
delete_header = ["10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"]
knn_header = [
    "k=1",
    "depth",
    "visNum",
    "k=10",
    "depth",
    "visNum",
    "k=100",
    "depth",
    "visNum",
]
count_header = ["100small", "100medium", "100large"]
rquery_header = ["100small", "100medium", "100large"]
file_header = {
    "build": build_header,
    "insert": insert_header,
    "delete": delete_header,
    "knn": knn_header,
    "count": count_header,
    "rquery": rquery_header,
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
    num = 2
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
