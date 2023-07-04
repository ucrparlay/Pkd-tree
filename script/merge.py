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
benchDim = "craft_var_dim"
benchNode = "craft_var_node"
storePrefix = "data/"
Nodes = [10000000, 50000000, 100000000, 500000000, 1000000000]
Dims = [2, 3, 5, 7, 9]
header = [
    "solver",
    "benchType",
    "nodes",
    "dims",
    "file",
    "buildTime",
    "queryTime",
    "wrapSize",
]


def combine(P, csvWriter, solver, benchName, node, dim):
    lines = open(P, "r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(" ")
        if solver == "cgal":
            csvWriter.writerow([solver, benchName, node, dim, l[0], l[1], l[2], -1])
        else:
            csvWriter.writerow([solver, benchName, node, dim, l[0], l[1], l[2], l[3]])


def check(P):
    lines = open(P, "r").readlines()
    for line in lines:
        l = " ".join(line.split())
        l = l.split(" ")
        print(l[-1])
        if l[-1] != "ok":
            print("wrong", P)


def csvSetup(solver):
    csvFilePointer = open(storePrefix + solver + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(header)
    return csvWriter


# * merge the result
if len(sys.argv) > 1 and int(sys.argv[1]) == 1:
    solverName = ["my_kd", "cgal"]
    resMap = {"my_kd": "res.out", "cgal": "cgal_res.out"}

    for solver in solverName:
        csvWriter = csvSetup(solver)

        dim = 3
        for node in Nodes:
            P = (
                path
                + "/"
                + benchNode
                + "/"
                + str(node)
                + "_"
                + str(dim)
                + "/"
                + resMap[solver]
            )
            combine(P, csvWriter, solver, benchNode, node, dim)

        # node = 100000000
        # for dim in Dims:
        #     P = (
        #         path
        #         + "/"
        #         + benchDim
        #         + "/"
        #         + str(node)
        #         + "_"
        #         + str(dim)
        #         + "/"
        #         + resMap[solver]
        #     )
        #     combine(P, csvWriter, solver, benchDim, node, dim)


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

# * check the correctness
if len(sys.argv) > 3 and int(sys.argv[3]) == 1:
    node = 100000
    for dim in Dims:
        P = (
            path
            + "/"
            + benchDim
            + "/"
            + str(node)
            + "_"
            + str(dim)
            + "/"
            + "Correct.out"
        )
        check(P)

    dim = 5
    for node in Nodes:
        P = (
            path
            + "/"
            + benchNode
            + "/"
            + str(node)
            + "_"
            + str(dim)
            + "/"
            + "Correct.out"
        )
        check(P)
