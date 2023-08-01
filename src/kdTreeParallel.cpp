#include "kdTreeParallel.h"

//@ Find Bounding Box

//@ Support Functions
template<typename point>
inline coord ParallelKDtree<point>::ppDistanceSquared( const point& p, const point& q,
                                                       const uint_fast8_t& DIM ) {
  coord r = 0;
  for ( int i = 0; i < DIM; i++ ) {
    r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
  }
  return r;
}

//@ cut and then rotate
template<typename point>
void ParallelKDtree<point>::divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim,
                                           int idx, int deep, int& bucket,
                                           const uint_fast8_t& DIM ) {
  if ( deep > BUILD_DEPTH_ONCE ) {
    //! sometimes cut dimension can be -1
    //! never use pivots[idx].first to check whether it is in bucket; instead,
    //! use idx > PIVOT_NUM
    pivots[idx] = splitter( -1, bucket++ );
    return;
  }
  int n = In.size();
  std::nth_element(
      In.begin(), In.begin() + n / 2, In.end(),
      [&]( const point& p1, const point& p2 ) { return p1.pnt[dim] < p2.pnt[dim]; } );
  pivots[idx] = splitter( In[n / 2].pnt[dim], dim );
  dim = ( dim + 1 ) % DIM;
  divide_rotate( In.cut( 0, n / 2 ), pivots, dim, 2 * idx, deep + 1, bucket, DIM );
  divide_rotate( In.cut( n / 2, n ), pivots, dim, 2 * idx + 1, deep + 1, bucket, DIM );
  return;
}

//@ starting at dimesion dim and pick pivots in a rotation manner
template<typename point>
void ParallelKDtree<point>::pick_pivots( slice In, const size_t& n, splitter_s& pivots,
                                         const uint_fast8_t& dim,
                                         const uint_fast8_t& DIM ) {
  size_t size = std::min( n, (size_t)32 * BUCKET_NUM );
  assert( size < n );
  points arr = points::uninitialized( size );
  for ( size_t i = 0; i < size; i++ ) {
    arr[i] = In[i * ( n / size )];
  }
  pivots = splitter_s::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
  int bucket = 0;
  divide_rotate( arr.cut( 0, size ), pivots, dim, 1, 1, bucket, DIM );
  assert( bucket == BUCKET_NUM );
  return;
}

template<typename point>
inline uint_fast32_t ParallelKDtree<point>::find_bucket( const point& p,
                                                         const splitter_s& pivots,
                                                         const uint_fast8_t& dim,
                                                         const uint_fast8_t& DIM ) {
  uint_fast16_t k = 1, d = dim;
  while ( k <= PIVOT_NUM ) {
    assert( d == pivots[k].second );
    k = p.pnt[d] < pivots[k].first ? k << 1 : k << 1 | 1;
    d = ( d + 1 ) % DIM;
  }
  assert( pivots[k].first == -1 );
  return pivots[k].second;
}

template<typename point>
void ParallelKDtree<point>::partition( slice A, slice B, const size_t& n,
                                       const splitter_s& pivots,
                                       parlay::sequence<uint_fast32_t>& sums,
                                       const uint_fast8_t& dim,
                                       const uint_fast8_t& DIM ) {
  size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
  parlay::sequence<parlay::sequence<uint_fast32_t>> offset(
      num_block, parlay::sequence<uint_fast32_t>( BUCKET_NUM ) );
  assert( offset.size() == num_block && offset[0].size() == BUCKET_NUM &&
          offset[0][0] == 0 );
  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      offset[i][std::move( find_bucket( A[j], pivots, dim, DIM ) )]++;
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

  // todo try change to counting sort
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
      B[v[std::move( find_bucket( A[j], pivots, dim, DIM ) )]++] = A[j];
    }
  } );

  return;
}

template<typename point>
NODE<point>* ParallelKDtree<point>::build_inner_tree(
    uint_fast16_t idx, splitter_s& pivots, parlay::sequence<node*>& treeNodes ) {
  if ( idx > PIVOT_NUM ) {
    assert( idx - PIVOT_NUM - 1 < BUCKET_NUM );
    return treeNodes[idx - PIVOT_NUM - 1];
  }
  node *L, *R;
  L = build_inner_tree( idx << 1, pivots, treeNodes );
  R = build_inner_tree( idx << 1 | 1, pivots, treeNodes );
  return alloc_interior_node( L, R, pivots[idx], pivots[idx].second );
}

//@ Parallel KD tree cores
template<typename point>
void ParallelKDtree<point>::build( slice A, const uint_fast8_t& DIM ) {
  points B = points::uninitialized( A.size() );
  this->root = build_recursive( A, B.cut( 0, A.size() ), 0, DIM );
  assert( this->root != NULL );
  return;
}

template<typename point>
NODE<point>* ParallelKDtree<point>::build_recursive( slice In, slice Out,
                                                     uint_fast8_t dim,
                                                     const uint_fast8_t& DIM ) {
  size_t n = In.size();

  if ( n <= LEAVE_WRAP ) {
    return alloc_leaf_node( In, dim );
  }

  //* serial run nth element
  if ( n <= SERIAL_BUILD_CUTOFF ) {
    std::nth_element(
        In.begin(), In.begin() + n / 2, In.end(),
        [&]( const point& p1, const point& p2 ) { return p1.pnt[dim] < p2.pnt[dim]; } );
    splitter split = splitter( In[n / 2].pnt[dim], dim );
    dim = ( dim + 1 ) % DIM;
    node *L, *R;
    L = build_recursive( In.cut( 0, n / 2 ), Out.cut( 0, n / 2 ), dim, DIM );
    R = build_recursive( In.cut( n / 2, n ), Out.cut( n / 2, n ), dim, DIM );
    return alloc_interior_node( L, R, split, split.second );
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
        for ( int j = 0; j < i; j++ ) {
          start += sums[j];
        }
        treeNodes[i] = build_recursive( Out.cut( start, start + sums[i] ),
                                        In.cut( start, start + sums[i] ), dim, DIM );
      },
      1 );
  return build_inner_tree( 1, pivots, treeNodes );
}

template<typename point>
void ParallelKDtree<point>::flatten( NODE<point>* T, slice Out ) {
  assert( T->size == Out.size() );
  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      Out[i] = TL->pts[i];
    }
    return;
  }
  interior* TI = static_cast<interior*>( T );
  if ( TI->size != TI->left->size + TI->right->size ) {
    LOG << TI->size << " " << TI->right->size << " " << TI->left->size << ENDL;
  }
  assert( TI->size == TI->left->size + TI->right->size );
  parlay::par_do_if(
      TI->size > SERIAL_BUILD_CUTOFF,
      [&]() { flatten( TI->left, Out.cut( 0, TI->left->size ) ); },
      [&]() { flatten( TI->right, Out.cut( TI->left->size, TI->size ) ); } );

  return;
}

template<typename point>
inline void ParallelKDtree<point>::update_interior( NODE<point>* T, NODE<point>* L,
                                                    NODE<point>* R ) {
  assert( !T->is_leaf );
  interior* TI = static_cast<interior*>( T );
  TI->size = L->size + R->size;
  TI->left = L;
  TI->right = R;
  return;
}

// template<typename point>
// void ParallelKDtree<point>::assign_node_tag( node_tags& tags, node* T, uint_fast8_t
// dim,
//                                              const uint_fast8_t& DIM, int deep, int
//                                              idx, int& bucket ) {
//     if ( T->is_leaf || deep > BUILD_DEPTH_ONCE ) {
//         assert( bucket < BUCKET_NUM );
//         tags[idx] = node_tag( T, bucket++ );
//         return;
//     }
//     //* BUCKET ID in [0, BUCKET_NUM)
//     tags[idx] = node_tag( T, BUCKET_NUM );
//     interior* TI = static_cast<interior*>( T );
//     dim = ( dim + 1 ) % DIM;
//     assign_node_tag( tags, tnodes, TI->left, dim, DIM, deep + 1, idx << 1, bucket );
//     assign_node_tag( tags, tnodes, TI->right, dim, DIM, deep + 1, idx << 1 | 1, bucket
//     ); return;
// }

template<typename point>
uint_fast32_t ParallelKDtree<point>::retrive_tag( const point& p, const node_tags& tags,
                                                  const uint_fast8_t& dim,
                                                  const uint_fast8_t& DIM ) {
  uint_fast32_t k = 1, d = dim;
  interior* TI;

  while ( k <= PIVOT_NUM && ( !tags[k].first->is_leaf ) ) {
    TI = static_cast<interior*>( tags[k].first );
    assert( TI->split.second == d );
    k = p.pnt[d] < TI->split.first ? k << 1 : k << 1 | 1;
    d = ( d + 1 ) % DIM;
  }
  assert( tags[k].second < BUCKET_NUM );
  return tags[k].second;
}

template<typename point>
void ParallelKDtree<point>::seieve_points( slice A, slice B, const size_t& n,
                                           const node_tags& tags,
                                           parlay::sequence<uint_fast32_t>& sums,
                                           const uint_fast8_t& dim,
                                           const uint_fast8_t& DIM, const int& tagsNum,
                                           const bool seieve ) {
  size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
  parlay::sequence<parlay::sequence<uint_fast32_t>> offset(
      num_block, parlay::sequence<uint_fast32_t>( tagsNum ) );
  assert( offset.size() == num_block && offset[0].size() == tagsNum &&
          offset[0][0] == 0 );
  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    uint_fast32_t k;
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      k = retrive_tag( A[j], tags, dim, DIM );
      offset[i][k]++;
    }
  } );

  sums = parlay::sequence<uint_fast32_t>( tagsNum );
  for ( size_t i = 0; i < num_block; i++ ) {
    auto t = offset[i];
    offset[i] = sums;
    for ( int j = 0; j < tagsNum; j++ ) {
      sums[j] += t[j];
    }
  }

  if ( !seieve ) {  //* just calculate candidate
    return;
  }

  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    auto v = parlay::sequence<uint_fast32_t>::uninitialized( tagsNum );
    int tot = 0, s_offset = 0;
    for ( int k = 0; k < tagsNum - 1; k++ ) {
      v[k] = tot + offset[i][k];
      tot += sums[k];
      s_offset += offset[i][k];
    }
    v[tagsNum - 1] = tot + ( ( i << LOG2_BASE ) - s_offset );
    uint_fast32_t k;
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      k = retrive_tag( A[j], tags, dim, DIM );
      B[v[k]++] = A[j];
    }
  } );

  return;
}

template<typename point>
NODE<point>* ParallelKDtree<point>::update_inner_tree( uint_fast32_t idx,
                                                       const node_tags& tags,
                                                       parlay::sequence<node*>& treeNodes,
                                                       int& p,
                                                       const tag_nodes& rev_tag ) {
  //! order below matters
  //! tree nodes can be in bucket, so be sure to release space first
  if ( tags[idx].first->is_leaf ) {
    assert( rev_tag[p] == idx );
    if ( !treeNodes[p]->is_leaf ) {
      assert( tags[idx].first != NULL );
      assert( tags[idx].first != treeNodes[p] );
      free_leaf( tags[idx].first );
    }
    return treeNodes[p++];
  }

  if ( tags[idx].second > BUCKET_NUM ) {
    assert( rev_tag[p] == idx );
    assert( tags[idx].second == BUCKET_NUM + 1 );
    assert( !( tags[idx].first->is_leaf ) );
    assert( tags[idx].first != treeNodes[p] );
    delete_tree_recursive( tags[idx].first );
    return treeNodes[p++];
  }

  if ( idx > PIVOT_NUM ) {
    assert( rev_tag[p] == idx );
    return treeNodes[p++];
  }

  assert( tags[idx].second == BUCKET_NUM );
  node *L, *R;
  L = update_inner_tree( idx << 1, tags, treeNodes, p, rev_tag );
  R = update_inner_tree( idx << 1 | 1, tags, treeNodes, p, rev_tag );
  update_interior( tags[idx].first, L, R );
  return tags[idx].first;
}

//* return the updated node
template<typename point>
NODE<point>* ParallelKDtree<point>::batchInsert_recusive( node* T, slice In, slice Out,
                                                          uint_fast8_t dim,
                                                          const uint_fast8_t& DIM ) {
  size_t n = In.size();

  if ( n == 0 ) return T;

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    if ( n + TL->size <= LEAVE_WRAP ) {
      assert( T->size == TL->size );
      for ( int i = 0; i < n; i++ ) {
        TL->pts[TL->size + i] = In[i];
      }
      TL->size += n;
      return T;
    } else {
      points wx = points::uninitialized( n + TL->size );
      points wo = points::uninitialized( n + TL->size );
      for ( int i = 0; i < TL->size; i++ ) {
        wx[i] = TL->pts[i];
      }
      parlay::parallel_for(
          0, n, [&]( size_t i ) { wx[TL->size + i] = In[i]; }, BLOCK_SIZE );
      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), dim,
                              DIM );
    }
  }

  if ( n <= SERIAL_BUILD_CUTOFF ) {
    interior* TI = static_cast<interior*>( T );

    assert( dim == TI->split.second );
    auto pos = std::partition( In.begin(), In.end(), [&]( const point& p ) {
      return p.pnt[dim] < TI->split.first;
    } );
    int len = pos - In.begin();
    assert( len == std::distance( In.begin(), pos ) );
    //* rebuild
    if ( Gt( std::abs( 100.0 * ( TI->left->size + len ) / ( TI->size + n ) - 50 ),
             INBALANCE_RATIO * 1.0 ) ) {
      points wx = points::uninitialized( T->size + In.size() );
      points wo = points::uninitialized( T->size + In.size() );
      // LOG << "begin serial flatten" << ENDL;
      flatten( T, wx.cut( 0, T->size ) );
      // LOG << "END serial flatten" << ENDL;
      parlay::parallel_for(
          0, n, [&]( size_t j ) { wx[T->size + j] = In[j]; }, BLOCK_SIZE );
      // delete_tree_recursive( T );
      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), dim,
                              DIM );
    }
    //* continue
    dim = ( dim + 1 ) % DIM;
    node *L, *R;
    L = batchInsert_recusive( TI->left, In.cut( 0, len ), Out.cut( 0, len ), dim, DIM );
    R = batchInsert_recusive( TI->right, In.cut( len, n ), Out.cut( len, n ), dim, DIM );
    update_interior( T, L, R );
    assert( T->size == L->size + R->size && TI->split.second >= 0 &&
            TI->is_leaf == false );
    return T;
  }

  //@ assign each node a tag
  InnerTree IT;
  assert( IT.rev_tag.size() == BUCKET_NUM );
  IT.reset_tags_num();
  IT.assign_node_tag( T, dim, DIM, 1, 1 );
  assert( IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM );

  seieve_points( In, Out, n, IT.tags, IT.sums, dim, DIM, IT.tagsNum, 1 );

  IT.tag_inbalance_node();
  assert( IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM );
  auto treeNodes = parlay::sequence<node*>::uninitialized( IT.tagsNum );
  parlay::parallel_for(
      0, IT.tagsNum,
      [&]( size_t i ) {
        size_t s = 0;
        for ( int j = 0; j < i; j++ ) {
          s += IT.sums_tree[IT.rev_tag[j]];
        }

        if ( IT.tags[IT.rev_tag[i]].second < BUCKET_NUM ) {  //* continue sieve
          treeNodes[i] = batchInsert_recusive(
              IT.tags[IT.rev_tag[i]].first, Out.cut( s, s + IT.sums_tree[IT.rev_tag[i]] ),
              In.cut( s, s + IT.sums_tree[IT.rev_tag[i]] ),
              IT.tags[IT.rev_tag[i]].first->dim, DIM );
        } else {  //* launch rebuild subtree
          assert( IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 1 );
          points wx = points::uninitialized( IT.tags[IT.rev_tag[i]].first->size +
                                             IT.sums_tree[IT.rev_tag[i]] );
          points wo = points::uninitialized( IT.tags[IT.rev_tag[i]].first->size +
                                             IT.sums_tree[IT.rev_tag[i]] );
          flatten( IT.tags[IT.rev_tag[i]].first,
                   wx.cut( 0, IT.tags[IT.rev_tag[i]].first->size ) );
          parlay::parallel_for(
              0, IT.sums_tree[IT.rev_tag[i]],
              [&]( size_t j ) {
                wx[IT.tags[IT.rev_tag[i]].first->size + j] = Out[s + j];
              },
              BLOCK_SIZE );
          treeNodes[i] = build_recursive( parlay::make_slice( wx ),
                                          parlay::make_slice( wo ), dim, DIM );
        }
      },
      1 );

  int beatles = 0;
  return update_inner_tree( 1, IT.tags, treeNodes, beatles, IT.rev_tag );
}

template<typename point>
void ParallelKDtree<point>::batchInsert( slice A, const uint_fast8_t& DIM ) {
  points B = points::uninitialized( A.size() );
  node* T = this->root;
  this->root = batchInsert_recusive( T, A, B.cut( 0, A.size() ), 0, DIM );
  assert( this->root != NULL );
  return;
}

//? parallel query
template<typename point>
void ParallelKDtree<point>::k_nearest( node* T, const point& q, const uint_fast8_t& DIM,
                                       kBoundedQueue<coord>& bq, size_t& visNodeNum ) {
  visNodeNum++;

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      bq.insert( std::move( ppDistanceSquared( q, TL->pts[i], DIM ) ) );
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  auto dx = [&]() { return TI->split.first - q.pnt[TI->split.second]; };

  k_nearest( dx() > 0 ? TI->left : TI->right, q, DIM, bq, visNodeNum );
  if ( dx() * dx() > bq.top() && bq.full() ) {
    return;
  }
  k_nearest( dx() > 0 ? TI->right : TI->left, q, DIM, bq, visNodeNum );
}

template<typename point>
NODE<point>* ParallelKDtree<point>::delete_tree() {
  if ( this->root == NULL ) {
    return this->root;
  }
  delete_tree_recursive( this->root );
  assert( this->root != NULL );
  return this->root;
}

template<typename point>  //* delete tree in parallel
void ParallelKDtree<point>::delete_tree_recursive( node* T ) {
  if ( T->is_leaf ) {
    free_leaf( T );
  } else {
    interior* TI = static_cast<interior*>( T );
    parlay::par_do_if(
        T->size > SERIAL_BUILD_CUTOFF, [&] { delete_tree_recursive( TI->left ); },
        [&] { delete_tree_recursive( TI->right ); } );
    free_interior( T );
  }
}

//@ Template declation
template class ParallelKDtree<point2D>;
template class ParallelKDtree<point3D>;
template class ParallelKDtree<point5D>;
template class ParallelKDtree<point7D>;
template class ParallelKDtree<point9D>;
template class ParallelKDtree<point10D>;