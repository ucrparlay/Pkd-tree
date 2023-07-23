#include "kdTreeParallel.h"

//@ Find Bounding Box

//@ Support Functions
template <typename point>
inline coord
ParallelKDtree<point>::ppDistanceSquared( const point& p, const point& q,
                                          const int& DIM ) {
   coord r = 0;
   for( int i = 0; i < DIM; i++ ) {
      r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
   }
   return r;
}

//@ cut and then rotate
template <typename point>
void
ParallelKDtree<point>::divide_rotate( slice In, splitter_s& pivots, int dim,
                                      int idx, int deep, int& bucket,
                                      const int& DIM ) {
   if( deep > BUILD_DEPTH_ONCE ) {
      //! sometimes splitter can be -1
      //! never use pivots[idx].first to check whether it is in bucket; instead,
      //! use idx > PIVOT_NUM
      pivots[idx] = splitter( -1, bucket++ );
      return;
   }
   int n = In.size();
   std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                     [&]( const point& p1, const point& p2 ) {
                        return p1.pnt[dim] < p2.pnt[dim];
                     } );
   pivots[idx] = splitter( In[n / 2].pnt[dim], dim );
   dim = ( dim + 1 ) % DIM;
   divide_rotate( In.cut( 0, n / 2 ), pivots, dim, 2 * idx, deep + 1, bucket,
                  DIM );
   divide_rotate( In.cut( n / 2, n ), pivots, dim, 2 * idx + 1, deep + 1,
                  bucket, DIM );
   return;
}

//@ starting at dimesion dim and pick pivots in a rotation manner
template <typename point>
void
ParallelKDtree<point>::pick_pivots( slice In, const size_t& n,
                                    splitter_s& pivots, const int& dim,
                                    const int& DIM ) {
   size_t size = std::min( n, (size_t)32 * BUCKET_NUM );
   assert( size < n );
   points arr = points::uninitialized( size );
   for( size_t i = 0; i < size; i++ ) {
      arr[i] = In[i * ( n / size )];
   }
   pivots = splitter_s::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
   int bucket = 0;
   divide_rotate( arr.cut( 0, size ), pivots, dim, 1, 1, bucket, DIM );
   assert( bucket == BUCKET_NUM );
   return;
}

template <typename point>
inline uint_fast32_t
ParallelKDtree<point>::find_bucket( const point& p, const splitter_s& pivots,
                                    const int& dim, const int& DIM ) {
   uint_fast16_t k = 1, d = dim;
   while( k <= PIVOT_NUM ) {
      assert( d == pivots[k].second );
      k = p.pnt[d] < pivots[k].first ? k << 1 : k << 1 | 1;
      d = ( d + 1 ) % DIM;
   }
   assert( pivots[k].first == -1 );
   return pivots[k].second;
}

template <typename point>
void
ParallelKDtree<point>::partition( slice A, slice B, const size_t& n,
                                  const splitter_s& pivots,
                                  parlay::sequence<uint_fast32_t>& sums,
                                  const int& dim, const int& DIM ) {
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

   return;
}

template <typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                                         parlay::sequence<node*>& treeNodes ) {
   if( idx > PIVOT_NUM ) {
      assert( idx - PIVOT_NUM - 1 < BUCKET_NUM );
      return treeNodes[idx - PIVOT_NUM - 1];
   }
   node *L, *R;
   L = build_inner_tree( idx << 1, pivots, treeNodes );
   R = build_inner_tree( idx << 1 | 1, pivots, treeNodes );
   // return interior_allocator.allocate( L, R, pivots[idx] );
   return alloc_interior_node( L, R, pivots[idx] );
}

//@ Parallel KD tree cores
template <typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::build( slice In, slice Out, int dim, const int& DIM ) {
   size_t n = In.size();

   if( n <= LEAVE_WRAP ) {
      return alloc_leaf_node( In );
      // return leaf_allocator.allocate( In );
   }

   //* serial run nth element
   if( n <= SERIAL_BUILD_CUTOFF ) {
      std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                        [&]( const point& p1, const point& p2 ) {
                           return p1.pnt[dim] < p2.pnt[dim];
                        } );
      splitter split = splitter( In[n / 2].pnt[dim], dim );
      dim = ( dim + 1 ) % DIM;
      node *L, *R;
      L = build( In.cut( 0, n / 2 ), Out.cut( 0, n / 2 ), dim, DIM );
      R = build( In.cut( n / 2, n ), Out.cut( n / 2, n ), dim, DIM );
      // return interior_allocator.allocate( L, R, split );
      return alloc_interior_node( L, R, split );
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
template <typename point>
void
ParallelKDtree<point>::k_nearest( node* T, const point& q, const int& DIM,
                                  kBoundedQueue<coord>& bq,
                                  size_t& visNodeNum ) {
   visNodeNum++;

   if( T->is_leaf ) {
      leaf* TL = static_cast<leaf*>( T );
      for( int i = 0; i < TL->size; i++ ) {
         bq.insert( std::move( ppDistanceSquared( q, TL->pts[i], DIM ) ) );
      }
      return;
   }

   interior* TI = static_cast<interior*>( T );
   auto dx = [&]() { return TI->split.first - q.pnt[TI->split.second]; };

   k_nearest( dx() > 0 ? TI->left : TI->right, q, DIM, bq, visNodeNum );
   if( dx() * dx() > bq.top() && bq.full() ) {
      return;
   }
   k_nearest( dx() > 0 ? TI->right : TI->left, q, DIM, bq, visNodeNum );
}

template <typename point>
void
ParallelKDtree<point>::delete_tree( node* T ) { //* delete tree in parallel
   if( T->is_leaf ) {
      leaf_allocator.retire( static_cast<leaf*>( T ) );
   } else {
      interior* TI = static_cast<interior*>( T );
      parlay::par_do_if(
          T->size > 1000, [&] { delete_tree( TI->left ); },
          [&] { delete_tree( TI->right ); } );
      interior_allocator.retire( TI );
   }
}

//@ Template declation
template class ParallelKDtree<point2D>;
template class ParallelKDtree<point3D>;
template class ParallelKDtree<point5D>;
template class ParallelKDtree<point7D>;
template class ParallelKDtree<point9D>;
template class ParallelKDtree<point10D>;