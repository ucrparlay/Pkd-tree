# CPDD: Construction-based Parallel Dynamic kD-tree

Requirements
--------
+ CMake >= 3.15 
+ g++ or clang with C++20 features support (Tested with clang 16.0.0) on Linux machines.
+ We use [ParlayLib](https://github.com/cmuparlay/parlaylib) to support fork-join parallelism and some parallel primitives. It is provided as a submodule in our repository. 
+ We use [CGAL](https://www.cgal.org/index.html) to perform benchmark comparison.

Getting Code
--------
Clone the repository with submodules
```bash
git clone --recurse-submodules git@github.com:ucrparlay/KDtree.git
cd KDtree
```
Compilation
--------
```bash
mkdir build && cd build
cmake -DDEBUG=OFF ..
make
```
Usage
--------
```bash
./test -p <file_name> -d <point_dimension> -t <0:build, 1:build+insert, 2:build+insert+delete> \
 -r <test_rounds> -q <query_type>
```

Test Framework Format
--------
The test starts with $n$ input points $P$, and $\alpha\cdot n$ points $Q$ for insertion and deletion, where $\alpha\in[0,1]$. We also have one integer $t\in\{0,1,2\}$ marks tree state before query and another integer $q$ stands for the type of query.

All outputs should be in one line, starts with the input file name and seperated by a single space. If any output item is unavaliable (i.e., tree depth w.r.t the BDL tree), outputs `-1` instead.

The execution flow is shown below:

1. Function `buildTree (t>=0)` builds a $k$d-tree $T$ over $P$. 
Outputs construction time and average tree depth separated by a single space.
2. Function `insert (t>=1)` insert $Q[0,\cdots, \alpha\cdot n]$ into $T$.
Outputs instertion time.
3. Function `delete (t>=2)` delete $Q[0,\cdots, \alpha\cdot n]$ from $T$.
Outputs delete time.
4. Query `q & (1<<0)` asks KNN of $P$ on $T$ where $k=1, 10, 100$. 
For each KNN, outputs time for query, average depth and average # nodes visited.
5. Query `q & (1<<1)` asks 10-NN w.r.t points $P'=\{P_0,\cdots,P_{1e6}\}$ (assume $|P|\geq 1e6$). 
For each KNN, outputs time for query, average depth and average # nodes visited.
6. Query `q & (1<<2)` asks range count on $T$ each with size $n = [0,n^{1/4}）, [n^{1/4}, n^{1/2}), [n^{1/2}, n)$
For each query, outputs time for that query.
7. Query `q & (1<<3)` asks range query on $T$ with size above.
Same outputs as in 6.
8. Update `q & (1<<4)` insert $Q$ into $T$ incrementally with step $10\%$.
For each step, outputs the total time for such insertion. 
9. Update `q & (1<<5)` delete $P$ from $T$ incrementally with step $10\%$.
For each step, outputs the total time for such deletion. 
10. Update `q & (1<<6)` constrcut $T$ from $P$ incrementally using steps $[0.1, 0.2, 0.25, 0.5]$
For each step, outputs the total construction time and the average tree depth.
11. Update `q & (1<<7)` first construct a tree $T'$ using $P\cup Q$, then delete $Q$ from $T'$ incrementally using steps $[0.1, 0.2, 0.25, 0.5]$
For each step, outputs the deletion time and the average tree depth afterwards.
12. Query `q & (1<<8)` first constrtuct a tree $T$ using $P$ and run a 10-NN on it, then construct another tree $T'$ from $P$ incrementally and perform a 10-NN on it.
For each NN, outputs the total time, average depth and average # nodes visited.
13. Query `q & (1<<9)` first constrtuct a tree $T$ using $P$ and run a 10-NN on it, then construct another tree $T'$ from $P\cup Q$, delete $Q$ from $T'$ incrementally, after which performs a 10-NN on it.
For each NN, outputs the total time, average depth and average # nodes visited.
