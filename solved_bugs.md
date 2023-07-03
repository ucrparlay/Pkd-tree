In memory of solved bugs. 
- Didn't return from leave wrap.
- The split passed to interior constructor is wrong, since the array order has been changed.
- The self-made input number is too large, resulting the long long overflow.
- Partition use n/2 for median split, rather than actual number which is smaller than the split.