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
benchmarks = ["ss_varden"]
# benchmarks = ["ss_varden", "uniform"]
storePrefix = "data/"
Nodes = [10000000, 50000000, 100000000, 500000000]
# Dims = [2, 3]
Dims = [3]
header = [
    "solver",
    "benchType",
    "nodes",
    "dims",
    "file",
    "build",
    "insert",
    "delete",
    "knn",
    "aveDeep",
    "aveVisNode",
    "rangeCount",
    "rangeQuery",
    "k=1",
    "aveDeep",
    "aveVisNode",
    "k=10",
    "aveDeep",
    "aveVisNode",
    "k=100",
    "aveDeep",
    "aveVisNode",
    "RQ0.05",
    "number",
    "RQ0.2",
    "number",
    "RQ0.5",
    "number",
    "batch10",
    "queryTime",
    "aveDeep",
    "aveVisNode",
    "batch30",
    "queryTime",
    "aveDeep",
    "aveVisNode",
    "batch50",
    "queryTime",
    "aveDeep",
    "aveVisNode",
    "batch100",
    "queryTime",
    "aveDeep",
    "aveVisNode",
]


def combine(P, csvWriter, solver, benchName, node, dim):
    if not os.path.isfile(P):
        print("No file fonund: " + P)
        return
    lines = open(P, "r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(" ")
        while len(l) < 37:
            l.append("-1")
        csvWriter.writerow([solver, benchName, node, dim] + l)


def csvSetup(solver):
    csvFilePointer = open(storePrefix + solver + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(header)
    return csvWriter


# * merge the result
if len(sys.argv) > 1 and int(sys.argv[1]) == 1:
    # solverName = ["test", "zdtree", "cgal"]
    solverName = ["test", "zdtree"]
    resMap = {"test": "res.out", "cgal": "cgal.out", "zdtree": "zdtree.out"}

    csvWriter = csvSetup("result")

    for dim in Dims:
        for solver in solverName:
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


if len(sys.argv) > 1 and int(sys.argv[1]) == 2:
    Nodes = [100000000, 500000000]
    cores = [1, 4, 8, 16, 24, 48, 96]

    solverName = ["test"]
    resMap = {"test": "res.out"}
    csvFilePointer = open(storePrefix + "scalability" + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(
        [
            "solver",
            "benchType",
            "nodes",
            "dims",
            "file",
            "buildTime",
            "insertTime",
            "deleteTime",
            "core",
        ]
    )
    for solver in solverName:
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
                        + "_"
                        + resMap[solver]
                    )
                    if not os.path.isfile(P):
                        print("No file fonund: " + P)
                        continue
                    lines = open(P, "r").readlines()
                    for line in lines:
                        l = " ".join(line.split())
                        l = l.split(" ")
                        csvWriter.writerow(
                            [
                                solver,
                                bench,
                                node,
                                dim,
                                l[0],
                                l[1],
                                l[2],
                                l[3],
                                str(core),
                            ]
                        )
