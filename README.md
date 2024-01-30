# Pkd-tree: a parallel kd-tree with batch update

Pkd-tree is a library of $k$d-tree that:

- Surprisingly fast,
- Scale to billions input size,
- All operations are in parallel, including:
  - tree construction,
  - batch insertion/deletion,
  - $k$-NN,
  - range count/query.

## Requirements

Necessary:

- CMake >= 3.15
- g++ or clang with C++20 features support (Tested with g++13 and clang17) on Linux machines.
- We use [ParlayLib](https://github.com/cmuparlay/parlaylib) to support fork-join parallelism and some parallel primitives. It is provided as a submodule in our repository.

Optional:

- [jemalloc](https://github.com/jemalloc/jemalloc), slightly memory allocation improvement.
- [NUMA control](https://manpages.ubuntu.com/manpages/trusty/man8/numactl.8.html), improve the performance for parallelism.
- We use [Zdtree](https://github.com/cmuparlay/pbbsbench/tree/9820e9fc38ce64d43aa5c62aa02a0c3ec5384a92), [ParGeo](https://github.com/ParAlg/ParGeo) and [CGAL](https://www.cgal.org/index.html) to perform benchmark comparison.

## Getting Code

Try:

1. Clone the repository.
2. Initialize the submodule:

```bash
git submodule update --init
```

File structure:

```bash
.
├── benchmark
├── include
├── parlaylib
├── script
└── tests
```

|    Name     |          Usage          |
| :---------: | :---------------------: |
| `benchmark` | Stores sample benchmark |
|  `include`  |  Source of `Pkd-tree`   |
| `parlaylib` |   Provide parallelism   |
|  `scipts`   | Scripts for experiments |
|   `tests`   |   Helpers for testing   |

## Compilation

```bash
mkdir build && cd build
cmake -DDEBUG=OFF ..
make
```

Useful flags:

|   Name   |                 Usage                  | Default Value |
| :------: | :------------------------------------: | :-----------: |
| `DEBUG`  |         Compile in debug mode          |     `OFF`     |
| `SERIAL` |        Disable the parallelism         |     `OFF`     |
|  `CGAL`  | Compile all executable related to CGAL |     `OFF`     |
|  `JEMA`  |  Allocate the memory using `jemmaloc`  |     `OFF`     |

More options can be found in `CMakeLists.txt`.

## Usage

Implemented in `tests/test.cpp`. See also `tests/cgal.cpp` and `tests/zdtree/neighborsTime.C`.

### Command line

```bash
./test -p [input_path] -d [dimension] -t [batch_mode] \
 -r [rounds] -q [query_type] -i [read_insert_file_flag]
```

Parameters:

| Name |                       Usage                       |          Sample          |
| :--: | :-----------------------------------------------: | :----------------------: |
| `-p` |                 Input points path                 | `-p benchmark/sample.in` |
| `-d` |                Dimension of points                |          `-d 3`          |
| `-t` |  Batch mode, see [below](#test-framework-format)  |          `-t 0`          |
| `-r` |         How many rounds one test case run         |          `-r 3`          |
| `-q` | Query type, see [ below ](#test-framework-format) |          `-q 1`          |
| `-i` |     Whether to read the file for batch update     |          `-i 0`          |

### Example

Under `build/` folder, first generate two inputs, $P_1$ for tree construction and $P_2$ for batch update:

```{bash}
./data_generator ../benchmark 10000 3 2 0
```

which will generate two files named `1.in` and `2.in` in `benchmark/10000_3/`. Then to build the tree $P_1$, insert $P_2$, after which perform the $10$-NN search, try:

```{bash}
./test -p ../benchmark/10000_3/1.in -d 3 -t 1 -q 1 -r 3 -i 1
```

To parse the output, see [ section ](#test-framework-format) below.

### Default setting

In default, the Pkd-tree stores all coordinates of points in 64-bit integer (`long`). The balancing parameter is set to $0.3$, the leaf wrap is $32$. It builds $6$ levels of tree at once. Different values on different machine may influence the performance dramatically. See our paper for more explanation.

## Test Framework Format

Implemented in `tests/testFramework.h`.

The test starts with $n$ input points $P$, and $\alpha\cdot n$ points $Q$ for insertion and deletion, where $\alpha\in[0,1]$. We also have one integer $t\in\{0,1,2\}$ marks the mode for batch operation before query and another integer $q$ stands for the type of query.

All outputs should be in one line, starts with the input file name and separated by a single space. If any output item is unavailable, outputs `-1` instead.

The execution flow is shown below:

1. Function `buildTree (t>=0)` builds a $k$d-tree $T$ over $P$.
   Outputs construction time and average tree depth separated by a single space.

2. Function `insert (t>=1)` insert $Q[0,\cdots, \alpha\cdot n]$ into $T$.
   Outputs insertion time.

3. Function `delete (t>=2)` delete $Q[0,\cdots, \alpha\cdot n]$ from $T$.
   Outputs delete time.

4. Query `q & (1<<0)` asks KNN of $P$ on $T$.

   If $t=0$, then $k=\{1,10,100\}$, otherwise, $k=10$.
   For each KNN, outputs time for query, average depth and average # nodes visited.

5. Query `q & (1<<1)` asks 10-NN w.r.t points $P'=\{P_0,\cdots,P_{1e6}\}$ (assume $|P|\geq 1e6$).
   For each KNN, outputs time for query, average depth and average # nodes visited.

6. Query `q & (1<<2)` asks range count on $T$ each with size $n = [0,n^{1/4}）, [n^{1/4}, n^{1/2}), [n^{1/2}, n)$
   For each query, outputs time for that query.

7. Query `q & (1<<3)` asks range query on $T$ with size $n$.

   If $t=0$, then $n = [0,n^{1/4}）, [n^{1/4}, n^{1/2}), [n^{1/2}, n)$, otherwise, $n=[n^{1/2},n)$.
   For each query, outputs time for that query.

   Same outputs as in 6.

8. Update `q & (1<<4)` insert $Q$ into $T$ incrementally with step $10\%$. $\alpha=1$.
   For each step, outputs the total time for such insertion.

9. Update `q & (1<<5)` delete $P$ from $T$ incrementally with step $10\%$. $\alpha=1$.
   For each step, outputs the total time for such deletion.

10. Update `q & (1<<6)` construct $T$ from $P$ incrementally using steps $[0.1, 0.2, 0.25, 0.5]$
    For each step, outputs the total construction time and the average tree depth.

11. Update `q & (1<<7)` first construct a tree $T'$ using $P\cup Q$, then delete $Q$ from $T'$ incrementally using steps $[0.1, 0.2, 0.25, 0.5]$
    For each step, outputs the deletion time and the average tree depth afterwards.

12. Query `q & (1<<8)` first construct a tree $T$ using $P$ and run a 10-NN on it, then construct another tree $T'$ from $P$ incrementally and perform a 10-NN on it.
    For each NN, outputs the total time, average depth and average # nodes visited.

13. Query `q & (1<<9)` first construct a tree $T$ using $P$ and run a 10-NN on it, then construct another tree $T'$ from $P\cup Q$, delete $Q$ from $T'$ incrementally, after which performs a 10-NN on it.
    For each NN, outputs the total time, average depth and average # nodes visited.

## Comparison

- Zdtree

  1.  Clone the repository of [PBBS](https://github.com/cmuparlay/pbbsbench), then checkout the version [`9820e9f`](https://github.com/cmuparlay/pbbsbench/tree/9820e9fc38ce64d43aa5c62aa02a0c3ec5384a92).
  2.  Copy folder `tests/zdtree/octTree` to `pbbsbench/benchmarks/nearestNeighbors/`.
  3.  Copy file `tests/zdtree/neighborsTime.C` to `pbbsbench/benchmarks/nearestNeighbors/bench/`.
  4.  Copy file `tests/common/time_loop.h` to `pbbsbench/common/`.
  4.  Run the program according to the tutorial in PBBS.

- ParGeo
  
  See `ParGeo` branch for more details.

- CGAL
  1.  Make sure `CGAL` is installed.
  2.  Compile the whole program with the flag `-DCGAL=ON` attached.
  3.  There should be binary named `cgal` in `build/`, which can be run using the same argument as mentioned in [Usage](##usage) above.
