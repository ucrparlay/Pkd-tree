# KD tree

Requirements
--------
+ CMake >= 3.15 
+ g++ or clang with C++17 features support (Tested with clang 15.0.7) on Linux machines.
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
cmake ..
make
```

