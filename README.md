# CPDD: Construction-based Parallel Dynamic $k$D-tree

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