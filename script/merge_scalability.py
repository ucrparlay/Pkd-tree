import os
import sys
import csv

print(os.getcwd())

path = "../benchmark"
benchmarks = ["ss_varden", "uniform"]
storePrefix = "data/"

if len(sys.argv) > 1 and int(sys.argv[1]) == 1:
    Nodes = [1000000000]
    cores = [1, 2, 4, 8, 16, 24, 48, 96, 192]

    # solverName = ["test", "zdtree", "cgal"]
    solverName = ["test", "zdtree"]
    resMap = {"test": "res.out", "zdtree": "zdtree.out", "cgal": "cgal.out"}
    csvFilePointer = open(storePrefix + "scalability" + ".csv", "w", newline="")
    csvFilePointer.truncate()
    csvWriter = csv.writer(csvFilePointer)
    csvWriter.writerow(
        [
            "solver",
            "benchType",
            # "nodes",
            # "dims",
            # "file",
            "build",
            "depth",
            "insert",
            "delete",
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
                                # node,
                                # dim,
                                # l[0],
                                l[1],
                                l[2],
                                l[3],
                                l[4],
                                str(core),
                            ]
                        )
