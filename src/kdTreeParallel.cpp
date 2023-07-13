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

template <typename slice>
void
divide_rotate( slice In, splitter_s& pivots, int dim, int idx, int deep,
               int& bucket, const int& DIM ) {
   if( deep > BUILD_DEPTH_ONCE ) {
      //! todo sometimes splitter can be -1
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
splitter_s
pick_pivots( slice In, const size_t& n, const int& dim, const int& DIM ) {
   size_t size = (size_t)std::log2( n ) * PIVOT_NUM;
   assert( size < n );
   assert( n / size > 2.0 );
   points arr = points::uninitialized( size );
   for( size_t i = 0; i < size; i++ ) {
      arr[i] = In[i * ( n / size )];
   }
   // todo pass reference
   splitter_s pivots = splitter_s::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
   int bucket = 0;
   divide_rotate( arr.cut( 0, size ), pivots, dim, 1, 1, bucket, DIM );
   assert( bucket == BUCKET_NUM );
   return pivots;
}

inline int
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
std::array<uint32_t, BUCKET_NUM>
partition( slice A, slice B, const size_t& n, const splitter_s& pivots,
           const int& dim, const int& DIM ) {
   size_t num_block = ( n + BLOCK_SIZE - 1 ) >> log2_base;
   auto offset = new std::array<uint32_t, BUCKET_NUM>[num_block] {};
   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      int k;
      for( size_t j = i << log2_base; j < std::min( ( i + 1 ) << log2_base, n );
           j++ ) {
         // for( int k = 0; k < PIVOT_NUM; k++ ) {
         //    if( A[j].pnt[dim] < pivots[k] ) {
         //       offset[i][k]++;
         //       break;
         //    }
         // }
         k = find_bucket( A[j], pivots, dim, DIM );
         offset[i][k]++;
      }
   } );

   std::array<uint32_t, BUCKET_NUM> sums{};
   for( size_t i = 0; i < num_block; i++ ) {
      auto t = offset[i];
      offset[i] = sums;
      for( int j = 0; j < BUCKET_NUM; j++ ) {
         sums[j] += t[j];
      }
   }
   parlay::parallel_for( 0, num_block, [&]( size_t i ) {
      std::array<uint32_t, BUCKET_NUM> v;
      int tot = 0, s_offset = 0;
      for( int k = 0; k < BUCKET_NUM - 1; k++ ) {
         v[k] = tot + offset[i][k];
         tot += sums[k];
         s_offset += offset[i][k];
      }
      v[BUCKET_NUM - 1] = tot + ( ( i << log2_base ) - s_offset );
      for( size_t j = i << log2_base; j < std::min( ( i + 1 ) << log2_base, n );
           j++ ) {
         int k = find_bucket( A[j], pivots, dim, DIM );
         B[v[k]++] = A[j];
      }
   } );

   delete offset;
   // todo pass by reference
   return sums;
}

inline size_t
count_left_right( int k, const splitter_s& pivots,
                  const std::array<uint32_t, BUCKET_NUM>& sums ) {
   if( k > PIVOT_NUM ) {
      assert( k - PIVOT_NUM - 1 < BUCKET_NUM );
      return sums[pivots[k].second];
   }
   return count_left_right( k * 2, pivots, sums ) +
          count_left_right( k * 2 + 1, pivots, sums );
}

//@ Parallel KD tree cores
template <typename slice>
node*
build( slice In, slice Out, int dim, const int& DIM, splitter_s pivots,
       int pivotIndex, std::array<uint32_t, BUCKET_NUM> sums ) {
   size_t n = In.size(), cut = 0;
   coord split = -1;
   int cut_dim = -1;

   if( n <= LEAVE_WRAP ) {
      return leaf_allocator.allocate( parlay::to_sequence( In ) );
   }

   if( n <= SERIAL_BUILD_CUTOFF ) { //* serial run nth element
      if( pivotIndex > PIVOT_NUM ) {
         std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                           pointLess( dim ) );
         cut = n / 2;
         split = In[n / 2].pnt[dim];
         pivotIndex = PIVOT_NUM + 1;
      }
      std::swap( In, Out );
   } else { //* parallel partitons
      if( pivotIndex > PIVOT_NUM ) {
         pivots = pick_pivots( In, n, dim, DIM );

         auto legal_pivots = [&]() {
            int td = dim;
            for( int i = 1; i <= PIVOT_NUM; i++ ) {
               if( pivots[i].second != dim )
                  return false;
               td = ( td + 1 ) % DIM;
            }
            for( int i = PIVOT_NUM + 1; i < PIVOT_NUM + BUCKET_NUM + 1; i++ ) {
               if( pivots[i].first != -1 )
                  return false;
            }
            return true;
         };
         assert( legal_pivots() );

         pivotIndex = 1;
         sums = partition( In, Out, n, pivots, dim, DIM );
         // pivots = pivots.pop_tail( BUCKET_NUM );
         assert( In.size() != 0 && Out.size() != 0 );
      } else {
         std::swap( In, Out );
      }
   }

   if( pivotIndex <= PIVOT_NUM ) {
      // puts( "already divided" );
      assert( pivots[pivotIndex].second == dim );
      split = pivots[pivotIndex].first;
      cut = count_left_right( pivotIndex * 2, pivots, sums );
      assert( cut + count_left_right( pivotIndex * 2 + 1, pivots, sums ) == n );
   }

   auto good_partition = [&]() {
      if( n <= SERIAL_BUILD_CUTOFF )
         return true;
      bool flag = true;
      // parlay::parallel_for( 0, cut, [&]( size_t i ) {
      //    if( Out[i].pnt[dim] >= split ) {
      //       __sync_bool_compare_and_swap( &flag, true, false );
      //    }
      // } );
      for( int i = 0; i < cut; i++ ) {
         if( Out[i].pnt[dim] >= split ) {
            flag = false;
            LOG << "should be less" << Out[i].pnt[dim] << " " << split << ENDL;
            break;
         }
      }
      // parlay::parallel_for( cut, n, [&]( size_t i ) {
      //    if( Out[i].pnt[dim] < split ) {
      //       __sync_bool_compare_and_swap( &flag, true, false );
      //    }
      // } );
      for( int i = 0; i < cut; i++ ) {
         if( Out[i].pnt[dim] >= split ) {
            flag = false;
            LOG << "should be greater-eq" << Out[i].pnt[dim] << " " << split
                << ENDL;
            break;
         }
      }
      // if( flag == false ) {
      //    for( int i = 0; i < n; i++ ) {
      //       LOG << Out[i].pnt[dim] << " ";
      //    }
      //    LOG << "cut num: " << cut << ENDL;
      //    if( n <= SERIAL_BUILD_CUTOFF ) {
      //       puts( "in serial" );
      //       LOG << n << ENDL;
      //    } else
      //       puts( "in parallel" );
      // }
      return flag;
   };
   assert( good_partition() );

   node *L, *R;
   cut_dim = dim;
   dim = ( dim + 1 ) % DIM;
   // L = build( Out.cut( 0, cut ), In.cut( 0, cut ), dim, DIM, pivots,
   //            2 * pivotIndex, sums );
   // R = build( Out.cut( cut, n ), In.cut( cut, n ), dim, DIM, pivots,
   //            2 * pivotIndex + 1, sums );
   parlay::par_do_if(
       n > SERIAL_BUILD_CUTOFF,
       [&]() {
          L = build( Out.cut( 0, cut ), In.cut( 0, cut ), dim, DIM, pivots,
                     2 * pivotIndex, sums );
       },
       [&]() {
          R = build( Out.cut( cut, n ), In.cut( cut, n ), dim, DIM, pivots,
                     2 * pivotIndex + 1, sums );
       } );
   return interior_allocator.allocate( L, R, split, cut_dim );
}

//? parallel query
void
k_nearest( node* T, const point& q, const int& DIM, kBoundedQueue<coord>& bq,
           size_t& visNodeNum ) {
   visNodeNum++;

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

   k_nearest( dx > 0 ? TI->left : TI->right, q, DIM, bq, visNodeNum );
   if( dx2 > bq.top() && bq.full() ) {
      return;
   }
   k_nearest( dx > 0 ? TI->right : TI->left, q, DIM, bq, visNodeNum );
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
                                      splitter_s pivots, int pivotIndex,
                                      std::array<uint32_t, BUCKET_NUM> sums );
