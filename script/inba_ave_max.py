import sys
import re

average = 0
maxrange = 0
p_alpha = re.compile(r"alpha: ([0-9]*)")


def process_file(filename):
    lines = []
    try:
        with open(filename, "r") as file:
            lines = file.readlines()
    except FileNotFoundError:
        print(f"The file {filename} does not exist.")
    except Exception as e:
        print(f"An error occurred: {e}")

    alpha = 0
    data = []
    res = []
    for l in lines:
        s = re.search(p_alpha, l)
        if s is not None:
            alpha = int(s.group(1))
            continue

        if l.strip() == "":
            res.append((alpha, data))
            data = []
            continue

        data.append([float(k) for k in l.split()])

    return res


if __name__ == "__main__":
    filename = sys.argv[1]
    res = process_file(filename)
    for alpha, data in res:
        print("alpha: %d " % alpha, end="")
        cols = [2, -6, -4, -2]
        for c in cols:
            avg_r = sum(r[c] for r in data) / len(data)
            max_r = max(r[c] for r in data)
            print("%.6f %.6f " % (avg_r, max_r), end="")
        print("")
