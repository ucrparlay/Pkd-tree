# Edit Distance
This repository contains code for our paper "Efficient Parallel Output-Sensitive Edit Distance".

Requirements
--------
+ CMake >= 3.15 
+ g++ or clang with C++17 features support (Tested with g++ 12.1.1 and clang 14.0.6) on Linux machines.
+ We use [ParlayLib](https://github.com/cmuparlay/parlaylib) to support fork-join parallelism and some parallel primitives. It is provided as a submodule in our repository. 

Getting Code
--------
Clone the repository with submodules
```bash
git clone --recurse-submodules https://github.com/ucrparlay/Edit-Distance.git
cd Edit-Distance
```
Compilation
--------
```bash
mkdir build && cd build
cmake ..
make
```

Running Code
--------
For synthetic dataset:
```
./test_framework <id> <n> <k> <sigma> <rounds>
```
+ id: id of the algorithm  
    + id=-1 all of our algorithms [BFS-Hash, BFS-H-Hash, BFS-SA, DaC-SP]
    + id=0: BFS-Hash
    + id=1: BFS-H-Hash
    + id=2: BFS-SA
    + id=3: DaC-SP
    + id=4: Sequential DP
    + id=5: ParlayLib
+ n: estimated length of strings  
+ k: estimated number of edits  
+ sigma: alphabet size  
+ rounds: number of rounds  

For example, to run all our algorithms on $n=10^9$ and $k=10$:
```
./test_framework -1 1000000000 10 256
```

For real-world dataset:
```
./test_framework_real -i <id> -f1 <path to file1> -f2 <path to file2> 
```

Reference
--------
Xiangyun Ding, Xiaojun Dong, Yan Gu, Youzhe Liu and Yihan Sun. Efficient Parallel Output-Sensitive Edit Distance. In submission.
