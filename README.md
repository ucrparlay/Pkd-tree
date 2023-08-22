# Parallel Batch Dynamic KD tree

Requirements
--------
+ CMake >= 3.15 
+ g++ or clang with C++20 features support (Tested with clang 15.0.7) on Linux machines.
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
Known Issues
--------
- If the insertion points are highly duplicated, then applying median of cutting dimension in `build_recursive()` may incur `segmentation fault`. For example, assuming five 1-d points `1 1 1 1 2`, the median is `1` and the split position is `0`, which indicates that all points would be partitioned to the right. The next recursion would perform the exactly same procedure above and run out of the stack memory in the end. 

