- 8 bit = 1 byte, one 3 dimension point (64-bit) uses $8*3=24$ byte, one leaf containing
  32 points uses $24*32$ byte; L3 cache is 36MB in dynamic, equivalent to 36*(2^20)
  byte, therefore, a small tree contains $(36/24)*2^{20}\approx 1572864$ points can be fitted in
  cache. An input graph containing 10^8 points can be divided to $63.5$ buckets each
  with amount of points above.
- add id in point can be much slow
- simply apply n_th element is not enough since the two sides of split may contain exactly same elements. This is okay for insert/build, since the following look-up will always correct. But if we want to look for a point by split this is wrong, i.e., 1 2 2 3 2, if the query point has dimension 2 we will miss the direction. 