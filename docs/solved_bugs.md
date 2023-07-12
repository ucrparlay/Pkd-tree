In memory of solved bugs. 
- `WA`: Didn't return from leave wrap.
- `WA`: The split passed to interior constructor is wrong, since the array order has been changed.
- `WA`: The self-made input number is too large, resulting the long long overflow.
- `WA`: Partition use n/2 for median split, rather than actual number which is smaller than the split.
- `RE`: Forget to update split value after parallel; use NEW same name variable within if.
- `WA`: When build multiple layers of tree at once, the cutting dimension will remain unchanged within these levels.
- `Not-Bug`: When use `n-th element` on $k$-th dimension, sometimes both left and right would contain points with same coordiantes in dimension $k$. This is ok, sicne the KD tree will ONLY skip the right subtree if the distance to the cutting plane is larger than the best candidate. It is same to do so. 





