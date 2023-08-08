- 8 bit = 1 byte, one 3 dimension point (64-bit) uses $8*3=24$ byte, one leaf containing
  32 points uses $24*32$ byte; L3 cache is 36MB in dynamic, equivalent to 36*(2^20)
  byte, therefore, a small tree contains $(36/24)*2^{20}\approx 1572864$ points can be fitted in
  cache. An input graph containing 10^8 points can be divided to $63.5$ buckets each
  with amount of points above.
- add id in point can be much slow