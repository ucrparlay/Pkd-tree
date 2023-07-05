In memory of solved bugs. 
- `WA`: Didn't return from leave wrap.
- `WA`: The split passed to interior constructor is wrong, since the array order has been changed.
- `WA`: The self-made input number is too large, resulting the long long overflow.
- `WA`: Partition use n/2 for median split, rather than actual number which is smaller than the split.
- `RE`: Forget to update split value after parallel; use NEW same name variable within if.

1 2 3 4 5 6 7

2 1 4 5 7 6 3, #1 pivot 6
2 1 4 5 3 6 = 7 
random pivot 
sample 4 6 -> 4 
# 2 pivots 4 6
2 1 3 =4= 5 =6= 7




