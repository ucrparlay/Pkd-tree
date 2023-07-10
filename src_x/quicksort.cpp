#include <algorithm>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
using namespace std;
constexpr int NUM_SAMPLES = 100;
constexpr int THRESHOLD = 1 << 18;
constexpr int log2_base = 12;
constexpr int BLOCK_SIZE = 1 << log2_base;
constexpr int np = 16; // num_pivot

inline uint32_t
hash32( uint32_t a ) {
   a = ( a + 0x7ed55d16 ) + ( a << 12 );
   a = ( a ^ 0xc761c23c ) ^ ( a >> 19 );
   a = ( a + 0x165667b1 ) + ( a << 5 );
   a = ( a + 0xd3a2646c ) ^ ( a << 9 );
   a = ( a + 0xfd7046c5 ) + ( a << 3 );
   a = ( a ^ 0xb55a4f09 ) ^ ( a >> 16 );
   return a;
}

template <typename T>
array<T, np>
pick_pivot( T* A, size_t n ) {
   int size = np << 1 | 1; //* 2*np+1 size
   T arr[size];
   for( int i = 0; i < size; i++ ) {
      arr[i] = A[i * ( n / size )]; //* sample in A
   }
   sort( arr, arr + size );
   array<T, np> pivots;
   for( int i = 0; i < np; i++ ) {
      pivots[i] = arr[i << 1 | 1]; //* pick np within cadidates
   }
   if( arr[0] == arr[1] ) { // x disturbtion
      pivots[0] = pivots[1];
   } else if( arr[3] == arr[4] ) {
      pivots[1] = pivots[0];
   }
   return pivots;
}

template <typename T>
array<int, np>
partition( T* A, T* B, size_t n, array<T, np>& pivots ) {
   size_t num_block = ( n + BLOCK_SIZE - 1 ) >> log2_base;
   //@ offset[i][k]:= # elements smaller than the k-th pivots in block i
   auto offset = new array<int, np>[num_block] {};
   cilk_for( size_t i = 0; i < num_block; i++ ) {
      for( size_t j = i << log2_base; j < min( ( i + 1 ) << log2_base, n );
           j++ ) {
         for( int k = 0; k < np; k++ ) {
            if( k == 0 ) {
               if( A[j] < pivots[k] ) {
                  offset[i][k]++;
                  break;
               }
            } else {
               if( A[j] <= pivots[k] ) {
                  offset[i][k]++;
                  break;
               }
            }
         }
      }
   }
   //* calculate all elements that is smaller than a pivot i
   //@ now offset is the cmulative # of elements smaller than k-th pivots in
   //@ first i-th block
   array<int, np> sums{};
   for( size_t i = 0; i < num_block; i++ ) {
      auto t = offset[i];
      offset[i] = sums;
      for( int j = 0; j < np; j++ ) {
         sums[j] += t[j];
      }
   }
   cilk_for( size_t i = 0; i < num_block; i++ ) {
      array<int, np + 1> v;
      int tot = 0, s_offset = 0;
      for( int k = 0; k < np; k++ ) {
         v[k] = tot + offset[i][k];
         tot += sums[k];
         s_offset += offset[i][k];
      }
      v[np] = tot + ( ( i << log2_base ) - s_offset );
      for( size_t j = i << log2_base; j < min( ( i + 1 ) << log2_base, n );
           j++ ) {
         int k;
         for( k = 0; k < np; k++ ) {
            if( k == 0 ) {
               if( A[j] < pivots[k] ) {
                  break;
               }
            } else {
               if( A[j] <= pivots[k] ) {
                  break;
               }
            }
         }
         B[v[k]++] = A[j];
      }
   }
   cilk_for( size_t i = 0; i < n; i++ ) { A[i] = B[i]; }
   delete offset;
   return sums;
}

template <typename T>
void
quicksort_( T* A, T* B, size_t n ) {
   if( n <= THRESHOLD ) {
      sort( A, A + n );
      return;
   }
   auto pivots = pick_pivot( A, n );
   auto sums = partition( A, B, n, pivots );
   int tot = sums[0];
   cilk_spawn quicksort_( A, B, sums[0] ); //* first block
   for( int i = 1; i < np; i++ ) {
      if( pivots[i] != pivots[i - 1] ) {
         cilk_spawn quicksort_( A + tot, B + tot, sums[i] );
      }
      tot += sums[i];
   }
   quicksort_( A + tot, B + tot, n - tot ); //* last block
   cilk_sync;
}

template <typename T>
void
quicksort( T* A, size_t n ) {
   T* B = new T[n];
   quicksort_( A, B, n );
   delete B;
}