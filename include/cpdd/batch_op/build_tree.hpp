#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
void
ParallelKDtree<point>::build( slice A, const uint_fast8_t DIM ) {
  points B = points::uninitialized( A.size() );
  this->bbox = get_box( A );
  this->root = build_recursive( A, B.cut( 0, A.size() ), 0, DIM, this->bbox );
  assert( this->root != NULL );
  return;
}

template<typename point>
void
ParallelKDtree<point>::divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim,
                                      int idx, int deep, int& bucket,
                                      const uint_fast8_t DIM, box_s& boxs,
                                      const box& bx ) {
  if ( deep > BUILD_DEPTH_ONCE ) {
    //! sometimes cut dimension can be -1
    //! never use pivots[idx].first to check whether it is in bucket; instead,
    //! use idx > PIVOT_NUM
    boxs[bucket] = bx;
    pivots[idx] = splitter( -1, bucket++ );
    return;
  }
  size_t n = In.size();
  uint_fast8_t d =
      ( _split_rule == MAX_STRETCH_DIM ? pick_max_stretch_dim( bx, DIM ) : dim );
  assert( d >= 0 && d < DIM );

  std::ranges::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                            [&]( const point& p1, const point& p2 ) {
                              return Num::Lt( p1.pnt[d], p2.pnt[d] );
                            } );
  pivots[idx] = splitter( In[n / 2].pnt[d], d );

  box lbox( bx ), rbox( bx );
  lbox.second.pnt[d] = pivots[idx].first;  //* loose
  rbox.first.pnt[d] = pivots[idx].first;

  d = ( d + 1 ) % DIM;
  divide_rotate( In.cut( 0, n / 2 ), pivots, d, 2 * idx, deep + 1, bucket, DIM, boxs,
                 lbox );
  divide_rotate( In.cut( n / 2, n ), pivots, d, 2 * idx + 1, deep + 1, bucket, DIM, boxs,
                 rbox );
  return;
}

//* starting at dimesion dim and pick pivots in a rotation manner
template<typename point>
void
ParallelKDtree<point>::pick_pivots( slice In, const size_t& n, splitter_s& pivots,
                                    const uint_fast8_t dim, const uint_fast8_t DIM,
                                    box_s& boxs, const box& bx ) {
  size_t size = std::min( n, (size_t)32 * BUCKET_NUM );
  assert( size <= n );
  points arr = points::uninitialized( size );
  for ( size_t i = 0; i < size; i++ ) {
    arr[i] = In[i * ( n / size )];
  }
  int bucket = 0;
  divide_rotate( arr.cut( 0, size ), pivots, dim, 1, 1, bucket, DIM, boxs, bx );
  assert( bucket == BUCKET_NUM );
  return;
}

template<typename point>
inline uint_fast8_t
ParallelKDtree<point>::find_bucket( const point& p, const splitter_s& pivots ) {
  uint_fast8_t k = 1;
  while ( k <= PIVOT_NUM ) {
    // k = p.pnt[pivots[k].second] < pivots[k].first ? k << 1 : k << 1 | 1;
    k = Num::Lt( p.pnt[pivots[k].second], pivots[k].first ) ? k << 1 : k << 1 | 1;
  }
  assert( pivots[k].first == -1 );
  return pivots[k].second;
}

template<typename point>
void
ParallelKDtree<point>::partition( slice A, slice B, const size_t& n,
                                  const splitter_s& pivots,
                                  parlay::sequence<uint_fast32_t>& sums ) {
  size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
  parlay::sequence<parlay::sequence<uint_fast32_t>> offset(
      num_block, parlay::sequence<uint_fast32_t>( BUCKET_NUM ) );
  assert( offset.size() == num_block && offset[0].size() == BUCKET_NUM &&
          offset[0][0] == 0 );
  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      offset[i][std::move( find_bucket( A[j], pivots ) )]++;
    }
  } );

  sums = parlay::sequence<uint_fast32_t>( BUCKET_NUM );
  for ( size_t i = 0; i < num_block; i++ ) {
    auto t = offset[i];
    offset[i] = sums;
    for ( int j = 0; j < BUCKET_NUM; j++ ) {
      sums[j] += t[j];
    }
  }

  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    auto v = parlay::sequence<uint_fast32_t>::uninitialized( BUCKET_NUM );
    int tot = 0, s_offset = 0;
    for ( int k = 0; k < BUCKET_NUM - 1; k++ ) {
      v[k] = tot + offset[i][k];
      tot += sums[k];
      s_offset += offset[i][k];
    }
    v[BUCKET_NUM - 1] = tot + ( ( i << LOG2_BASE ) - s_offset );
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      B[v[std::move( find_bucket( A[j], pivots ) )]++] = A[j];
    }
  } );

  return;
}

template<typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                                         parlay::sequence<node*>& treeNodes ) {
  if ( idx > PIVOT_NUM ) {
    assert( idx - PIVOT_NUM - 1 < BUCKET_NUM );
    return treeNodes[idx - PIVOT_NUM - 1];
  }
  node *L, *R;
  L = build_inner_tree( idx << 1, pivots, treeNodes );
  R = build_inner_tree( idx << 1 | 1, pivots, treeNodes );
  return alloc_interior_node( L, R, pivots[idx] );
}

template<typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::serial_build_recursive( slice In, slice Out, uint_fast8_t dim,
                                               const uint_fast8_t DIM, const box& bx ) {
  size_t n = In.size();

  if ( n <= LEAVE_WRAP ) {
    return alloc_leaf_node( In );
  }

  //* serial run nth element
  uint_fast8_t d =
      ( _split_rule == MAX_STRETCH_DIM ? pick_max_stretch_dim( bx, DIM ) : dim );
  assert( d >= 0 && d < DIM );

  std::ranges::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                            [&]( const point& p1, const point& p2 ) {
                              return Num::Lt( p1.pnt[d], p2.pnt[d] );
                            } );
  splitter split = splitter( In[n / 2].pnt[d], d );

  auto _2ndGroup = std::ranges::partition(
      In.begin(), In.begin() + n / 2,
      [&]( const point& p ) { return Num::Lt( p.pnt[split.second], split.first ); } );

  if ( _2ndGroup.begin() == In.begin() ) {
    // LOG << "sort right" << ENDL;
    assert( std::ranges::all_of( In.begin() + n / 2, In.end(),
                                 [&]( const point& p ) {
                                   return Num::Geq( p.pnt[split.second], split.first );
                                 } ) &&
            std::ranges::all_of( In.begin(), In.begin() + n / 2, [&]( const point& p ) {
              return Num::Eq( p.pnt[split.second], split.first );
            } ) );

    _2ndGroup = std::ranges::partition(
        In.begin() + n / 2, In.end(),
        [&]( const point& p ) { return Num::Eq( p.pnt[split.second], split.first ); } );
    assert( _2ndGroup.begin() - ( In.begin() + n / 2 ) > 0 );
    points_iter diffEleIter;

    if ( _2ndGroup.begin() != In.end() ) {
      //* need to change split
      auto minEleIter =
          std::ranges::min_element( _2ndGroup, [&]( const point& p1, const point& p2 ) {
            return Num::Lt( p1.pnt[split.second], p2.pnt[split.second] );
          } );
      assert( minEleIter != In.end() );

      split.first = minEleIter->pnt[split.second];

      assert( std::ranges::all_of( In.begin(), _2ndGroup.begin(),
                                   [&]( const point& p ) {
                                     return Num::Lt( p.pnt[split.second], split.first );
                                   } ) &&
              std::ranges::all_of( _2ndGroup, [&]( const point& p ) {
                return Num::Geq( p.pnt[split.second], split.first );
              } ) );
    } else if ( In.end() ==
                ( diffEleIter = std::ranges::find_if_not(
                      In, [&]( const point& p ) { return p == In[n / 2]; } ) ) ) {
      // LOG << "alloc dummy" << ENDL;
      assert( _2ndGroup.begin() == In.end() && diffEleIter == In.end() );
      return alloc_dummy_leaf( In );
    } else {  //* current dim d is same but other dims are not
      if ( _split_rule == MAX_STRETCH_DIM ) {  //* next recursion redirects to new dim
        return build_recursive( In, Out, d, DIM, get_box( In ) );
      } else if ( _split_rule == ROTATE_DIM ) {  //* switch dim, break rotation order
        points_iter compIter =
            diffEleIter == In.begin() ? std::ranges::prev( In.end() ) : In.begin();
        assert( compIter != diffEleIter );
        for ( int i = 0; i < DIM; i++ ) {
          // if ( diffEleIter->pnt[i] != compIter->pnt[i] ) {
          if ( !Num::Eq( diffEleIter->pnt[i], compIter->pnt[i] ) ) {
            d = i;
            break;
          }
        }
        return build_recursive( In, Out, d, DIM, bx );
      }
    }

    assert( !( _split_rule == MAX_STRETCH_DIM &&
               std::ranges::all_of( In.begin(), In.end(), [&]( const point& p ) {
                 return p == In[n / 2];
               } ) ) );
  }

  box lbox( bx ), rbox( bx );
  lbox.second.pnt[d] = split.first;  //* loose
  rbox.first.pnt[d] = split.first;

  d = ( d + 1 ) % DIM;
  node *L, *R;
  parlay::par_do_if(
      n > SERIAL_BUILD_CUTOFF,
      [&] {
        L = build_recursive( In.cut( 0, _2ndGroup.begin() - In.begin() ),
                             Out.cut( 0, _2ndGroup.begin() - In.begin() ), d, DIM, lbox );
      },
      [&] {
        R = build_recursive( In.cut( _2ndGroup.begin() - In.begin(), n ),
                             Out.cut( _2ndGroup.begin() - In.begin(), n ), d, DIM, rbox );
      } );
  return alloc_interior_node( L, R, split );
}

template<typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::build_recursive( slice In, slice Out, uint_fast8_t dim,
                                        const uint_fast8_t DIM, const box& bx ) {
  assert( In.size() == 0 || within_box( get_box( In ), bx ) );
  size_t n = In.size();

  if ( n <= SERIAL_BUILD_CUTOFF ) return serial_build_recursive( In, Out, dim, DIM, bx );

  //* parallel partitons
  auto pivots = splitter_s::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
  auto boxs = box_s::uninitialized( BUCKET_NUM );
  parlay::sequence<uint_fast32_t> sums;

  pick_pivots( In, n, pivots, dim, DIM, boxs, bx );
  partition( In, Out, n, pivots, sums );

  //* if random sampling failed to split points, re-partitions using serail approach
  if ( std::ranges::count( sums, 0 ) == BUCKET_NUM - 1 ) {
    LOG << "switch to serial" << ENDL;
    return serial_build_recursive( In, Out, dim, DIM, bx );
  }

  auto treeNodes = parlay::sequence<node*>::uninitialized( BUCKET_NUM );
  dim = ( dim + BUILD_DEPTH_ONCE ) % DIM;

  parlay::parallel_for(
      0, BUCKET_NUM,
      [&]( size_t i ) {
        size_t start = 0;
        for ( int j = 0; j < i; j++ ) {
          start += sums[j];
        }
        treeNodes[i] =
            build_recursive( Out.cut( start, start + sums[i] ),
                             In.cut( start, start + sums[i] ), dim, DIM, boxs[i] );
      },
      1 );
  return build_inner_tree( 1, pivots, treeNodes );
}

}  // namespace cpdd