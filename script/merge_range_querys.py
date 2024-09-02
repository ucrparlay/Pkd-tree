import os
import sys
import csv

print(os.getcwd())

path = "../benchmark"
benchmarks = ["uniform", "ss_varden"]
storePrefix = "data/"
Nodes = [1000000000]

type = "range_query_log"

#! order by test order

solverName = ["test", "cgal", "test_count", "LogTree", "BhlTree"]
files = ["build", "count"]
Dims = [3]

resMap = {
    "test": "res_" + type + ".out",
    "test_count": "res_" + "range_count_log" + ".out",
    "cgal": "cgal_" + type + ".out",
    "LogTree": "LogTree_" + type + ".out",
    "BhlTree": "BhlTree_" + type + ".out",
}

common = [
    "solver",
    "benchType",
    "nodes",
    "dims",
]
build_header = ["build", "depth"]
file_header = {
    "build": build_header,
}

prefix = [0] * len(files)


def get_recType(i):
    if i < 100:
        return 1
    if i < 200:
        return 2
    return 3


def combine(P, file, csvWriter, solver, benchName, node, dim):
    if not os.path.isfile(P):
        print("No file fonund: " + P)
        return

    lines = open(P, "r").readlines()
    sep_lines = []
    for line in lines:
        l = " ".join(line.split())
        l = l.split(" ")
        if len(l) == 0:
            continue
        sep_lines.append(l)

    sep_lines.pop(0)

    num = 1
    for i in range(0, len(sep_lines), num):
        csvWriter.writerow(
            [solver, benchName, node, dim] + sep_lines[i] + [get_recType(i)]
        )


def csvSetup(solver):
    print(solver)
    csvFilePointer = open(storePrefix + solver + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(
        ["solver", "benchmark", "node", "dim", "batchSize", "batchTime", "recType"]
    )
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


# * merge the result
if len(sys.argv) > 1 and int(sys.argv[1]) == 1:
    # print(type)
    # calculatePrefix()
    for file in files:
        csvWriter = csvSetup(file)

        for bench in benchmarks:
            for dim in Dims:
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
