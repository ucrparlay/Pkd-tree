# Optimization for CGAL::Kd_tree
- Use cache_friendly structure to reduce the cache miss. 
- Default splitter is defaults to [*Sliding_midpoint*](https://doc.cgal.org/latest/Spatial_searching/classCGAL_1_1Kd__tree.html). The `tree.build()` needs to be explicitly called in order to take effect.