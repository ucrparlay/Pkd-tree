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
ppDistanceSquared( const point& p, const point& q, const int& DIM ) {
   coord r = 0, diff = 0;
   for( int i = 0; i < DIM; i++ ) {
      diff = p.pnt[i] - q.pnt[i];
      r += diff * diff;
   }
   return r;
}

//@ cut and then rotate
template <typename slice>
void
divide_rotate( slice In, splitter_s& pivots, int dim, int idx, int deep,
               int& bucket, const int& DIM ) {
   if( deep > BUILD_DEPTH_ONCE ) {
      //! sometimes splitter can be -1
      //! never use pivots[idx].first to check whether it is in bucket; instead,
      //! use idx > PIVOT_NUM
      pivots[idx] = splitter( -1, bucket++ );
      return;
   }
   int n = In.size();
   std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                     pointLess( dim ) );
   pivots[idx] = splitter( In[n / 2].pnt[dim], dim );
   dim = ( dim + 1 ) % DIM;
   divide_rotate( In.cut( 0, n / 2 ), pivots, dim, 2 * idx, deep + 1, bucket,
                  DIM );
   divide_rotate( In.cut( n / 2, n ), pivots, dim, 2 * idx + 1, deep + 1,
                  bucket, DIM );
   return;
}

//@ starting at dimesion dim and pick pivots in a rotation manner
template <typename slice>
void
pick_pivots( slice In, const size_t& n, splitter_s& pivots, const int& dim,
             const int& DIM ) {
   size_t size = (size_t)std::log2( n ) * PIVOT_NUM;
   assert( size < n );
   assert( n / size > 2.0 );
   points arr = points::uninitialized( size );
   for( size_t i = 0; i < size; i++ ) {
      arr[i] = In[i * ( n / size )];
   }
   pivots = splitter_s::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
   int bucket = 0;
   divide_rotate( arr.cut( 0, size ), pivots, dim, 1, 1, bucket, DIM );
   assert( bucket == BUCKET_NUM );
   points().swap( arr );
   return;
}

inline uint_fast32_t
find_bucket( const point& p, const splitter_s& pivots, const int& dim,
             const int& DIM ) {
   int k = 1, d = dim;
   while( k <= PIVOT_NUM ) {
      assert( d == pivots[k].second );
      if( p.pnt[d] < pivots[k].first ) {
         k = k * 2;
      } else {
         k = k * 2 + 1;
      }
      d = ( d + 1 ) % DIM;
   }
   assert( pivots[k].first == -1 );
   return pivots[k].second;
}

template <typename slice>
void
partition( slice A, slice B, const size_t& n, const splitter_s& pivots,
           parlay::sequence<uint_fast32_t>& sums, const int& dim,
           const int& DIM ) {
   size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
   parlay::sequence<parlay::sequence<uint_fast32_t>> offset(
       num_block, parlay::sequence<uint_fast32_t>( BUCKET_NUM ) );
   assert( offset.size() == num_block && offset[0].size() == BUCKET_NUM &&
           offset[0][0] == 0 );
   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      for( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n );
           j++ ) {
         offset[i][std::move( find_bucket( A[j], pivots, dim, DIM ) )]++;
      }
   } );

   sums = parlay::sequence<uint_fast32_t>( BUCKET_NUM );
   for( size_t i = 0; i < num_block; i++ ) {
      auto t = offset[i];
      offset[i] = sums;
      for( int j = 0; j < BUCKET_NUM; j++ ) {
         sums[j] += t[j];
      }
   }

   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      auto v = parlay::sequence<uint_fast32_t>::uninitialized( BUCKET_NUM );
      int tot = 0, s_offset = 0;
      for( int k = 0; k < BUCKET_NUM - 1; k++ ) {
         v[k] = tot + offset[i][k];
         tot += sums[k];
         s_offset += offset[i][k];
      }
      v[BUCKET_NUM - 1] = tot + ( ( i << LOG2_BASE ) - s_offset );
      for( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n );
           j++ ) {
         B[v[std::move( find_bucket( A[j], pivots, dim, DIM ) )]++] = A[j];
      }
   } );

   decltype( offset )().swap( offset );
   return;
}

node*
build_inner_tree( uint_fast32_t idx, splitter_s& pivots,
                  parlay::sequence<node*>& treeNodes ) {
   if( idx > PIVOT_NUM ) {
      assert( idx - PIVOT_NUM - 1 < BUCKET_NUM );
      return treeNodes[idx - PIVOT_NUM - 1];
   }
   node *L, *R;
   L = build_inner_tree( idx * 2, pivots, treeNodes );
   R = build_inner_tree( idx * 2 + 1, pivots, treeNodes );
   return interior_allocator.allocate( L, R, pivots[idx] );
}

//@ Parallel KD tree cores
template <typename slice>
node*
build( slice In, slice Out, int dim, const int& DIM ) {
   size_t n = In.size();

   if( n <= LEAVE_WRAP ) {
      return leaf_allocator.allocate( std::move( parlay::to_sequence( In ) ) );
   }

   //* serial run nth element
   if( n <= SERIAL_BUILD_CUTOFF ) {
      std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                        pointLess( dim ) );
      size_t cut = n / 2;
      splitter split = splitter( In[n / 2].pnt[dim], dim );
      dim = ( dim + 1 ) % DIM;
      node *L, *R;
      L = build( In.cut( 0, cut ), Out.cut( 0, cut ), dim, DIM );
      R = build( In.cut( cut, n ), Out.cut( cut, n ), dim, DIM );
      return interior_allocator.allocate( L, R, split );
   }

   //* parallel partitons
   splitter_s pivots;
   parlay::sequence<uint_fast32_t> sums;
   pick_pivots( In, n, pivots, dim, DIM );
   partition( In, Out, n, pivots, sums, dim, DIM );
   auto treeNodes = parlay::sequence<node*>::uninitialized( BUCKET_NUM );
   dim = ( dim + BUILD_DEPTH_ONCE ) % DIM;
   parlay::parallel_for(
       0, BUCKET_NUM,
       [&]( size_t i ) {
          size_t start = 0;
          for( int j = 0; j < i; j++ ) {
             start += sums[j];
          }
          treeNodes[i] = build( Out.cut( start, start + sums[i] ),
                                In.cut( start, start + sums[i] ), dim, DIM );
       },
       1 );
   return build_inner_tree( 1, pivots, treeNodes );
}

//? parallel query
void
k_nearest( node* T, const point& q, const int& DIM, kBoundedQueue<coord>& bq,
           size_t& visNodeNum ) {
   visNodeNum++;

   // coord d, dx, dx2;
   if( T->is_leaf ) {
      leaf* TL = static_cast<leaf*>( T );
      for( int i = 0; i < TL->size; i++ ) {
         // d = ppDistanceSquared( q, TL->pts[i], DIM );
         // bq.insert( d );
         bq.insert( std::move( ppDistanceSquared( q, TL->pts[i], DIM ) ) );
      }
      return;
   }

   interior* TI = static_cast<interior*>( T );
   auto distance2Plane = [&]() -> size_t {
      return TI->split.first - q.pnt[TI->split.second];
   };

   // dx = TI->split.first - q.pnt[TI->split.second];
   // dx2 = dx * dx;

   k_nearest( TI->split.first - q.pnt[TI->split.second] > 0 ? TI->left
                                                            : TI->right,
              q, DIM, bq, visNodeNum );
   if( ( TI->split.first - q.pnt[TI->split.second] ) *
               ( TI->split.first - q.pnt[TI->split.second] ) >
           bq.top() &&
       bq.full() ) {
      return;
   }
   k_nearest( ( TI->split.first - q.pnt[TI->split.second] ) > 0 ? TI->right
                                                                : TI->left,
              q, DIM, bq, visNodeNum );
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
                                      int dim, const int& DIM );

//*----------------------------USELESS------------------------------------
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
pick_pivots_one_dimension( slice A, const size_t& n, const int& dim ) {
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
partition_one_dimension( slice A, slice B, const size_t& n,
                         const std::array<coord, PIVOT_NUM>& pivots,
                         const int& dim ) {
   size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
   auto offset = new std::array<int, PIVOT_NUM>[num_block] {};
   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      for( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n );
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
      v[PIVOT_NUM] = tot + ( ( i << LOG2_BASE ) - s_offset );
      for( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n );
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
