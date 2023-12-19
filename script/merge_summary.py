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
Dims = [2, 3]

solverName = ["zdtree", "test", "cgal"]
resMap = {
    "test": "res_summary.out",
    "cgal": "cgal_summary.out",
    "zdtree": "zdtree_summary.out",
}

common = [
    "solver",
    "benchType",
    "nodes",
    "dims",
]

#! order by test order
files = ["summary"]

build_header = [
    "build",
    "aveDepth",
    "insert",
    "delete",
    "k=10",
    "depth",
    "visNum",
]
file_header = {
    "summary": build_header,
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


def reorder():
    if not os.path.isfile("data/summary.csv"):
        print("No file fonund: " + P)
        return
    with open("../script/data/summary.csv", mode="r") as file:
        csvFile = csv.reader(file)
        for lines in csvFile:
            print(lines)


def csvSetup(solver):
    csvFilePointer = open(storePrefix + solver + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(common + file_header[file])
    return csvWriter, csvFilePointer


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
        csvWriter, csvFilePointer = csvSetup(file)
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
        csvFilePointer.close()

    # reorder()
