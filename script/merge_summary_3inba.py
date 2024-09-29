import os
import sys
import csv

print(os.getcwd())

path = "../benchmark"
# benchmarks = ["ss_varden"]
benchmarks = ["uniform", "ss_varden"]
storePrefix = "data/"
Nodes = [1000000000]
# Nodes = [100000000]
Dims = [2, 3, 5, 9]
# Dims = [12]

solverName = ["test"]
resMap = {"test": "res_summary_3inba.out"}

common = ["solver", "benchType", "nodes", "dims", "inba_ratios"]

#! order by test order
files = ["summary_3inba"]

build_header = [
    "build",
    "aveDepth",
    "insert_0.0001",
    "insert_0.001",
    "insert_0.01",
    "insert_0.1",
    "delete_0.0001",
    "delete_0.001",
    "delete_0.01",
    "delete_0.1",
    "k",
    "depth",
    "visNum",
    "rangeQuery",
]
file_header = {
    "summary_3inba": build_header,
}
inba_ratios = ["3", "10", "30"]

prefix = [0] * len(files)


def combine(P, file, csvWriter, solver, benchName, node, dim):
    if not os.path.isfile(P):
        print("No file fonund: " + P)
        if solver == "zdtree":
            csvWriter.writerow(
                [solver, benchName, node, dim] + ["-"] * len(build_header)
            )
        elif solver == "cgal":
            csvWriter.writerow(
                [solver, benchName, node, dim] + ["T"] * len(build_header)
            )
        return
    print(P)
    lines = open(P, "r").readlines()
    if len(lines) == 0:
        return
    for index, line in enumerate(lines):
        l = " ".join(line.split())
        l = l.split(" ")
        l[0] = inba_ratios[index]
        csvWriter.writerow([solver, benchName, node, dim] + l)


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
