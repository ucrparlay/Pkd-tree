#include "kdTreeParallel.h"

//@ Find Bounding Box
coords
center( box& b ) {
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = ( b.first[i] + b.second[i] ) / 2;
   return r;
}

box
bound_box( const parlay::sequence<point>& P ) {
   auto pts = parlay::map( P, []( point p ) { return p.pnt; } );
   auto x = box{ parlay::reduce( pts, parlay::binary_op( minv, coords() ) ),
                 parlay::reduce( pts, parlay::binary_op( maxv, coords() ) ) };
   return x;
}

box
bound_box( const box& b1, const box& b2 ) {
   return box{ minv( b1.first, b2.first ), maxv( b1.second, b2.second ) };
}

parlay::type_allocator<leaf> leaf_allocator;
parlay::type_allocator<interior> interior_allocator;

//@ Support Functions
inline coord
ppDistanceSquared( const point& p, const point& q, int DIM ) {
   coord r = 0, diff = 0;
   for( int i = 0; i < DIM; i++ ) {
      diff = p.pnt[i] - q.pnt[i];
      r += diff * diff;
   }
   return r;
}

template <typename slice>
coord
pick_single_pivot( slice A, const size_t& n, const int& dim ) {
   size_t size = PIVOT_NUM << 1 | 1;
   coord arr[size];
   for( size_t i = 0; i < size; i++ ) {
      arr[i] = A[i * ( n / size )].pnt[dim];
   }
   std::sort( arr, arr + size );
   std::array<coord, PIVOT_NUM> pivots;
   for( size_t i = 0; i < PIVOT_NUM; i++ ) {
      pivots[i] = arr[i << 1 | 1];
   }
   if( arr[0] == arr[1] ) {
      pivots[0] = pivots[1];
   } else if( arr[3] == arr[4] ) {
      pivots[1] = pivots[0];
   }
   return pivots[PIVOT_NUM >> 1];
}

template <typename slice>
std::array<coord, PIVOT_NUM>
pick_pivots( slice A, const size_t& n, const int& dim ) {
   size_t size = PIVOT_NUM << 1 | 1; //* 2*PIVOT_NUM+1 size
   coord arr[size];
   for( size_t i = 0; i < size; i++ ) {
      arr[i] = A[i * ( n / size )].pnt[dim]; //* sample cutting dimension in A
   }
   std::sort( arr, arr + size );
   std::array<coord, PIVOT_NUM> pivots;
   for( size_t i = 0; i < PIVOT_NUM; i++ ) {
      pivots[i] = arr[i << 1 | 1]; //* pick PIVOT_NUM within cadidates
   }
   if( arr[0] == arr[1] ) {
      pivots[0] = pivots[1];
   } else if( arr[3] == arr[4] ) {
      pivots[1] = pivots[0];
   }
   return pivots;
}

template <typename slice>
std::array<int, PIVOT_NUM>
partition( slice A, slice B, const size_t& n,
           const std::array<coord, PIVOT_NUM>& pivots, const int& dim ) {
   size_t num_block = ( n + BLOCK_SIZE - 1 ) >> log2_base;
   auto offset = new std::array<int, PIVOT_NUM>[num_block] {};
   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      for( size_t j = i << log2_base; j < std::min( ( i + 1 ) << log2_base, n );
           j++ ) {
         for( int k = 0; k < PIVOT_NUM; k++ ) {
            if( A[j].pnt[dim] < pivots[k] ) {
               offset[i][k]++;
               break;
            }
         }
      }
   } );

   std::array<int, PIVOT_NUM> sums{};
   for( size_t i = 0; i < num_block; i++ ) {
      auto t = offset[i];
      offset[i] = sums;
      for( int j = 0; j < PIVOT_NUM; j++ ) {
         sums[j] += t[j];
      }
   }
   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      std::array<int, PIVOT_NUM + 1> v;
      int tot = 0, s_offset = 0;
      for( int k = 0; k < PIVOT_NUM; k++ ) {
         v[k] = tot + offset[i][k];
         tot += sums[k];
         s_offset += offset[i][k];
      }
      v[PIVOT_NUM] = tot + ( ( i << log2_base ) - s_offset );
      for( size_t j = i << log2_base; j < std::min( ( i + 1 ) << log2_base, n );
           j++ ) {
         int k;
         for( k = 0; k < PIVOT_NUM; k++ ) {
            if( A[j].pnt[dim] < pivots[k] ) {
               break;
            }
         }
         B[v[k]++] = A[j];
      }
   } );
   delete offset;
   return sums;
}

//@ Parallel KD tree cores
template <typename slice>
node*
build( slice In, slice Out, int dim, const int& DIM,
       std::array<coord, PIVOT_NUM> pivots, int pn,
       std::array<int, PIVOT_NUM> sums ) {
   size_t n = In.size(), cut = 0;
   coord split;
   int cut_dim;
   size_t lpn = 0, rpn = 0;
   std::array<coord, PIVOT_NUM> lp, rp;
   std::array<int, PIVOT_NUM> lsum, rsum;

   if( n <= LEAVE_WRAP ) {
      return leaf_allocator.allocate( parlay::to_sequence( In ) );
   }
   if( n <= SERIAL_BUILD_CUTOFF ) { //* serial run nth element
      dim = ( dim + 1 ) % DIM;
      cut_dim = dim;
      std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                        pointLess( dim ) );
      cut = n / 2;
      split = In[n / 2].pnt[dim];
      std::swap( In, Out );
   } else { //* parallel partitons
      if( pn == 0 ) {
         dim = ( dim + 1 ) % DIM;
         cut_dim = dim;
         pivots = pick_pivots( In, n, dim );
         pn = PIVOT_NUM;
         sums = partition( In, Out, n, pivots, dim );
         assert( In.size() != 0 && Out.size() != 0 );
      } else {
         std::swap( In, Out );
         cut_dim = dim;
      }
      split = pivots[pn / 2];
      lpn = pn / 2;
      rpn = pn - lpn - 1;

      auto check = [&]() {
         bool flag = true;
         size_t tot = 0;
         for( int i = 0; i <= pn / 2; i++ ) {
            tot += sums[i];
         }
         parlay::parallel_for( 0, tot, [&]( size_t i ) {
            if( Out[i].pnt[cut_dim] >= split )
               __sync_bool_compare_and_swap( &flag, true, false );
         } );
         parlay::parallel_for( tot, n, [&]( size_t i ) {
            if( Out[i].pnt[cut_dim] < split )
               __sync_bool_compare_and_swap( &flag, true, false );
         } );
         return flag;
      };
      // assert( check() );

      for( int i = 0; i < lpn; i++ ) {
         cut += sums[i];
         lp[i] = pivots[i];
         lsum[i] = sums[i];
      }
      cut += sums[lpn];

      for( int i = 0; i < rpn; i++ ) {
         rp[i] = pivots[lpn + i + 1];
         rsum[i] = sums[lpn + i + 1];
      }
   }

   node *L, *R;
   parlay::par_do_if(
       n > SERIAL_BUILD_CUTOFF,
       [&]() {
          L = build( Out.cut( 0, cut ), In.cut( 0, cut ), dim, DIM, lp, lpn,
                     lsum );
       },
       [&]() {
          R = build( Out.cut( cut, n ), In.cut( cut, n ), dim, DIM, rp, rpn,
                     rsum );
       } );
   return interior_allocator.allocate( L, R, split, cut_dim );
}

//? parallel query
void
k_nearest( node* T, const point& q, const int DIM, kBoundedQueue<coord>& bq ) {
   coord d, dx, dx2;
   if( T->is_leaf ) {
      leaf* TL = static_cast<leaf*>( T );
      for( int i = 0; i < TL->size; i++ ) {
         d = ppDistanceSquared( q, TL->pts[i], DIM );
         bq.insert( d );
      }
      return;
   }

   interior* TI = static_cast<interior*>( T );
   int dim = TI->cut_dim;
   dx = TI->split - q.pnt[dim];
   dx2 = dx * dx;

   k_nearest( dx > 0 ? TI->left : TI->right, q, DIM, bq );
   if( dx2 > bq.top() && bq.full() ) {
      return;
   }
   k_nearest( dx > 0 ? TI->right : TI->left, q, DIM, bq );
}

void
delete_tree( node* T ) { //* delete tree in parallel
   if( T->is_leaf )
      leaf_allocator.retire( static_cast<leaf*>( T ) );
   else {
      interior* TI = static_cast<interior*>( T );
      parlay::par_do_if(
          T->size > 1000, [&] { delete_tree( TI->left ); },
          [&] { delete_tree( TI->right ); } );
      interior_allocator.retire( TI );
   }
}

//@ Template declation
template node*
build<parlay::slice<point*, point*>>( parlay::slice<point*, point*> In,
                                      parlay::slice<point*, point*> Out,
                                      int dim, const int& DIM,
                                      std::array<coord, PIVOT_NUM> pivots,
                                      int pn, std::array<int, PIVOT_NUM> sums );
