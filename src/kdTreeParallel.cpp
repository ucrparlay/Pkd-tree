#include "kdTreeParallel.h"

//@ Find Bounding Box

//@ Support Functions
template<typename point>
inline coord
ParallelKDtree<point>::ppDistanceSquared( const point& p, const point& q,
                                          const uint_fast8_t& DIM ) {
  coord r = 0;
  for ( int i = 0; i < DIM; i++ ) {
    r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
  }
  return std::move( r );
}

//@ cut and then rotate
template<typename point>
void
ParallelKDtree<point>::divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim,
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
void
ParallelKDtree<point>::pick_pivots( slice In, const size_t& n, splitter_s& pivots,
                                    const uint_fast8_t& dim, const uint_fast8_t& DIM ) {
  size_t size = std::min( n, (size_t)32 * BUCKET_NUM );
  assert( size <= n );
  // points arr = points::uninitialized( size );
  points arr = points( size );
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
inline uint_fast32_t
ParallelKDtree<point>::find_bucket( const point& p, const splitter_s& pivots,
                                    const uint_fast8_t& dim, const uint_fast8_t& DIM ) {
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
void
ParallelKDtree<point>::partition( slice A, slice B, const size_t& n,
                                  const splitter_s& pivots,
                                  parlay::sequence<uint_fast32_t>& sums,
                                  const uint_fast8_t& dim, const uint_fast8_t& DIM ) {
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
NODE<point>*
ParallelKDtree<point>::build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                                         parlay::sequence<node*>& treeNodes ) {
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
void
ParallelKDtree<point>::build( slice A, const uint_fast8_t& DIM ) {
  points B = points::uninitialized( A.size() );
  // box bx = get_box( A );
  this->root = build_recursive( A, B.cut( 0, A.size() ), 0, DIM );
  assert( this->root != NULL );
  return;
}

template<typename point>
NODE<point>*
ParallelKDtree<point>::build_recursive( slice In, slice Out, uint_fast8_t dim,
                                        const uint_fast8_t& DIM ) {
  if ( REMOVE_DUPLICATE_POINTS && In.size() <= SERIAL_BUILD_CUTOFF ) {
    std::sort( In.begin(), In.end() );
    auto p = std::unique( In.begin(), In.end() ) - In.begin();
    In = In.cut( 0, p );
  }

  size_t n = In.size();

  if ( n <= LEAVE_WRAP ) {
    return alloc_leaf_node( In, dim );
  }

  //* serial run nth element
  if ( n <= SERIAL_BUILD_CUTOFF ) {
    splitter split;
    auto pos = In.begin();

    if ( !REMOVE_DUPLICATE_POINTS ) {
      std::nth_element(
          In.begin(), In.begin() + n / 2, In.end(),
          [&]( const point& p1, const point& p2 ) { return p1.pnt[dim] < p2.pnt[dim]; } );
      split = splitter( In[n / 2].pnt[dim], dim );
      pos = std::partition( In.begin(), In.begin() + n / 2, [&]( const point& p ) {
        return p.pnt[split.second] < split.first;
      } );
    } else {
      split = splitter( In[n / 2].pnt[dim], dim );
      pos = In.begin() + n / 2;
    }

    // box lbox( bx ), rbox( bx );
    // lbox.second.pnt[d] = split.first;
    // rbox.first.pnt[d] = split.first;

    dim = ( dim + 1 ) % DIM;
    node *L, *R;
    L = build_recursive( In.cut( 0, pos - In.begin() ), Out.cut( 0, pos - In.begin() ),
                         dim, DIM );
    R = build_recursive( In.cut( pos - In.begin(), n ), Out.cut( pos - In.begin(), n ),
                         dim, DIM );
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
void
ParallelKDtree<point>::flatten( NODE<point>* T, slice Out ) {
  assert( T->size == Out.size() );
  if ( T->size == 0 ) return;

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      Out[i] = TL->pts[i];
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  assert( TI->size == TI->left->size + TI->right->size );
  parlay::par_do_if(
      TI->size > SERIAL_BUILD_CUTOFF,
      [&]() { flatten( TI->left, Out.cut( 0, TI->left->size ) ); },
      [&]() { flatten( TI->right, Out.cut( TI->left->size, TI->size ) ); } );

  return;
}

template<typename point>
void
ParallelKDtree<point>::flatten_and_delete( NODE<point>* T, slice Out ) {
  assert( T->size == Out.size() );
  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      Out[i] = TL->pts[i];
    }
    free_leaf( T );
    return;
  }

  interior* TI = static_cast<interior*>( T );
  assert( TI->size == TI->left->size + TI->right->size );
  parlay::par_do_if(
      TI->size > SERIAL_BUILD_CUTOFF,
      [&]() { flatten( TI->left, Out.cut( 0, TI->left->size ) ); },
      [&]() { flatten( TI->right, Out.cut( TI->size - TI->right->size, TI->size ) ); } );
  free_interior( T );

  return;
}

template<typename point>
inline void
ParallelKDtree<point>::update_interior( NODE<point>* T, NODE<point>* L, NODE<point>* R ) {
  assert( !T->is_leaf );
  interior* TI = static_cast<interior*>( T );
  TI->size = L->size + R->size;
  TI->left = L;
  TI->right = R;
  return;
}

template<typename point>
uint_fast32_t
ParallelKDtree<point>::retrive_tag( const point& p, const node_tags& tags ) {
  uint_fast32_t k = 1;
  interior* TI;

  while ( k <= PIVOT_NUM && ( !tags[k].first->is_leaf ) ) {
    TI = static_cast<interior*>( tags[k].first );
    k = p.pnt[TI->split.second] < TI->split.first ? k << 1 : k << 1 | 1;
  }
  assert( tags[k].second < BUCKET_NUM );
  return tags[k].second;
}

template<typename point>
void
ParallelKDtree<point>::seieve_points( slice A, slice B, const size_t& n,
                                      const node_tags& tags,
                                      parlay::sequence<uint_fast32_t>& sums,
                                      const int& tagsNum ) {
  size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
  parlay::sequence<parlay::sequence<uint_fast32_t>> offset(
      num_block, parlay::sequence<uint_fast32_t>( tagsNum ) );
  assert( offset.size() == num_block && offset[0].size() == tagsNum &&
          offset[0][0] == 0 );
  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    uint_fast32_t k;
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      k = retrive_tag( A[j], tags );
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
      k = retrive_tag( A[j], tags );
      B[v[k]++] = A[j];
    }
  } );

  return;
}

template<typename point>
NODE<point>*
ParallelKDtree<point>::update_inner_tree( uint_fast32_t idx, const node_tags& tags,
                                          parlay::sequence<node*>& treeNodes, int& p,
                                          const tag_nodes& rev_tag ) {

  if ( tags[idx].second == BUCKET_NUM + 1 || tags[idx].second == BUCKET_NUM + 2 ) {
    assert( rev_tag[p] == idx );
    return treeNodes[p++];
  }

  assert( tags[idx].second == BUCKET_NUM );
  assert( tags[idx].first != NULL );
  node *L, *R;
  L = update_inner_tree( idx << 1, tags, treeNodes, p, rev_tag );
  R = update_inner_tree( idx << 1 | 1, tags, treeNodes, p, rev_tag );
  update_interior( tags[idx].first, L, R );
  return tags[idx].first;
}

//* return the updated node
template<typename point>
NODE<point>*
ParallelKDtree<point>::batchInsert_recusive( node* T, slice In, slice Out,
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
      uint_fast8_t d = T->dim;
      free_leaf( T );
      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d,
                              DIM );
    }
  }

  if ( n <= SERIAL_BUILD_CUTOFF ) {
    interior* TI = static_cast<interior*>( T );
    auto pos = std::partition( In.begin(), In.end(), [&]( const point& p ) {
      return p.pnt[TI->split.second] < TI->split.first;
    } );
    assert( pos - In.begin() == std::distance( In.begin(), pos ) );

    //* rebuild
    if ( inbalance_node( TI->left->size + pos - In.begin(), TI->size + n ) ) {
      points wx = points::uninitialized( T->size + In.size() );
      points wo = points::uninitialized( T->size + In.size() );
      uint_fast8_t d = T->dim;
      size_t head_size = T->size;
      flatten( T, wx.cut( 0, T->size ) );
      delete_tree_recursive( T );

      parlay::parallel_for(
          0, n, [&]( size_t j ) { wx[head_size + j] = In[j]; }, BLOCK_SIZE );

      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d,
                              DIM );
    }
    //* continue
    node *L, *R;
    L = batchInsert_recusive( TI->left, In.cut( 0, pos - In.begin() ),
                              Out.cut( 0, pos - In.begin() ), DIM );
    R = batchInsert_recusive( TI->right, In.cut( pos - In.begin(), n ),
                              Out.cut( pos - In.begin(), n ), DIM );
    update_interior( T, L, R );
    assert( T->size == L->size + R->size && TI->split.second >= 0 &&
            TI->is_leaf == false );
    return T;
  }

  //@ assign each node a tag
  InnerTree IT;
  IT.init();
  assert( IT.rev_tag.size() == BUCKET_NUM );
  IT.assign_node_tag( T, 1 );
  assert( IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM );

  seieve_points( In, Out, n, IT.tags, IT.sums, IT.tagsNum );

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

        if ( IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 1 ) {  //* continue sieve
          treeNodes[i] = batchInsert_recusive(
              IT.tags[IT.rev_tag[i]].first, Out.cut( s, s + IT.sums_tree[IT.rev_tag[i]] ),
              In.cut( s, s + IT.sums_tree[IT.rev_tag[i]] ), DIM );
        } else {  //* launch rebuild subtree
          assert( IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 2 );
          assert( IT.tags[IT.rev_tag[i]].first->size + IT.sums_tree[IT.rev_tag[i]] >= 0 );
          points wx = points::uninitialized( IT.tags[IT.rev_tag[i]].first->size +
                                             IT.sums_tree[IT.rev_tag[i]] );
          points wo = points::uninitialized( IT.tags[IT.rev_tag[i]].first->size +
                                             IT.sums_tree[IT.rev_tag[i]] );
          uint_fast8_t d = IT.tags[IT.rev_tag[i]].first->dim;
          size_t head_size = IT.tags[IT.rev_tag[i]].first->size;

          flatten( IT.tags[IT.rev_tag[i]].first,
                   wx.cut( 0, IT.tags[IT.rev_tag[i]].first->size ) );
          delete_tree_recursive( IT.tags[IT.rev_tag[i]].first );

          parlay::parallel_for(
              0, IT.sums_tree[IT.rev_tag[i]],
              [&]( size_t j ) { wx[head_size + j] = Out[s + j]; }, BLOCK_SIZE );

          treeNodes[i] = build_recursive( parlay::make_slice( wx ),
                                          parlay::make_slice( wo ), d, DIM );
        }
      },
      1 );

  int beatles = 0;
  return update_inner_tree( 1, IT.tags, treeNodes, beatles, IT.rev_tag );
}

template<typename point>
void
ParallelKDtree<point>::batchInsert( slice A, const uint_fast8_t& DIM ) {
  points B = points::uninitialized( A.size() );
  node* T = this->root;
  this->root = batchInsert_recusive( T, A, B.cut( 0, A.size() ), DIM );
  assert( this->root != NULL );
  return;
}

template<typename point>
NODE<point>*
ParallelKDtree<point>::delete_inner_tree( uint_fast32_t idx, const node_tags& tags,
                                          parlay::sequence<node*>& treeNodes, int& p,
                                          const tag_nodes& rev_tag,
                                          const uint_fast8_t& DIM ) {
  if ( tags[idx].second == BUCKET_NUM + 1 || tags[idx].second == BUCKET_NUM + 2 ) {
    assert( rev_tag[p] == idx );
    return treeNodes[p++];
  }

  node *L, *R;
  L = delete_inner_tree( idx << 1, tags, treeNodes, p, rev_tag, DIM );
  R = delete_inner_tree( idx << 1 | 1, tags, treeNodes, p, rev_tag, DIM );
  update_interior( tags[idx].first, L, R );

  if ( tags[idx].second == BUCKET_NUM + 3 ) {
    interior* TI = static_cast<interior*>( tags[idx].first );
    assert( inbalance_node( TI->left->size, TI->size ) || TI->size < THIN_LEAVE_WRAP );
    if ( tags[idx].first->size == 0 ) {  //* special judge for empty tree
      uint_fast8_t d = tags[idx].first->dim;
      delete_tree_recursive( tags[idx].first );
      return alloc_leaf_node( points().cut( 0, 0 ), d );
    }
    points wx = points::uninitialized( tags[idx].first->size );
    points wo = points::uninitialized( tags[idx].first->size );
    uint_fast8_t d = tags[idx].first->dim;
    flatten( tags[idx].first, wx.cut( 0, tags[idx].first->size ) );
    delete_tree_recursive( tags[idx].first );
    return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM );
  }

  return tags[idx].first;
}

template<typename point>
NODE<point>*
ParallelKDtree<point>::batchDelete_recursive( node* T, slice In, slice Out,
                                              const uint_fast8_t& DIM, bool hasTomb ) {
  size_t n = In.size();

  if ( n == 0 ) return T;

  if ( n == T->size ) {
    if ( hasTomb ) {
      uint_fast8_t d = T->dim;
      delete_tree_recursive( T );
      return alloc_leaf_node( In.cut( 0, 0 ), d );
    }
    T->size = 0;  //* lazy mark
    return T;
  }

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    assert( T->size >= In.size() );
    auto it = TL->pts.begin(), end = TL->pts.begin() + TL->size;
    for ( int i = 0; i < In.size(); i++ ) {
      it = std::find( TL->pts.begin(), end, In[i] );
      assert( it != end );
      std::swap( *it, *( --end ) );
    }

    assert( std::distance( TL->pts.begin(), end ) == TL->size - In.size() );
    TL->size -= In.size();
    assert( TL->size >= 0 );
    return T;
  }

  if ( In.size() <= SERIAL_BUILD_CUTOFF ) {
    interior* TI = static_cast<interior*>( T );
    assert( TI->split.second == T->dim );
    auto pos = std::partition( In.begin(), In.end(), [&]( const point& p ) {
      return p.pnt[TI->split.second] < TI->split.first;
    } );

    bool putTomb = hasTomb && ( inbalance_node( TI->left->size - ( pos - In.begin() ),
                                                TI->size - In.size() ) ||
                                TI->size - In.size() < THIN_LEAVE_WRAP );
    hasTomb = putTomb ? false : hasTomb;
    assert( putTomb ? ( !hasTomb ) : true );

    node *L, *R;
    L = batchDelete_recursive( TI->left, In.cut( 0, pos - In.begin() ),
                               Out.cut( 0, pos - In.begin() ), DIM, hasTomb );
    R = batchDelete_recursive( TI->right, In.cut( pos - In.begin(), n ),
                               Out.cut( pos - In.begin(), n ), DIM, hasTomb );
    update_interior( T, L, R );
    assert( T->size == L->size + R->size && TI->split.second >= 0 &&
            TI->is_leaf == false );

    //* rebuild
    if ( putTomb ) {
      assert( TI->size == T->size );
      assert( inbalance_node( TI->left->size, TI->size ) || TI->size < THIN_LEAVE_WRAP );
      points wx = points::uninitialized( T->size );
      points wo = points::uninitialized( T->size );
      uint_fast8_t d = T->dim;
      flatten( T, wx.cut( 0, T->size ) );
      delete_tree_recursive( T );
      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d,
                              DIM );
    }

    return T;
  }

  InnerTree IT;
  IT.init();
  IT.assign_node_tag( T, 1 );
  assert( IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM );
  seieve_points( In, Out, n, IT.tags, IT.sums, IT.tagsNum );
  IT.tag_inbalance_node_deletion( hasTomb );

  auto treeNodes = parlay::sequence<node*>::uninitialized( IT.tagsNum );
  parlay::parallel_for(
      0, IT.tagsNum,
      [&]( size_t i ) {
        assert( IT.sums_tree[IT.rev_tag[i]] == IT.sums[i] );
        size_t start = 0;
        for ( int j = 0; j < i; j++ ) {
          start += IT.sums[j];
        }
        treeNodes[i] = batchDelete_recursive(
            IT.tags[IT.rev_tag[i]].first, Out.cut( start, start + IT.sums[i] ),
            In.cut( start, start + IT.sums[i] ), DIM,
            IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 1 );
      },
      1 );

  int beatles = 0;
  return delete_inner_tree( 1, IT.tags, treeNodes, beatles, IT.rev_tag, DIM );
}

template<typename point>
void
ParallelKDtree<point>::batchDelete( slice A, const uint_fast8_t& DIM ) {
  points B = points::uninitialized( A.size() );
  node* T = this->root;
  this->root = batchDelete_recursive( T, A, parlay::make_slice( B ), DIM, 1 );
  return;
}

//? parallel query
template<typename point>
void
ParallelKDtree<point>::k_nearest( node* T, const point& q, const uint_fast8_t& DIM,
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
NODE<point>*
ParallelKDtree<point>::delete_tree() {
  if ( this->root == nullptr ) {
    return this->root;
  }
  delete_tree_recursive( this->root );
  return this->root;
}

template<typename point>  //* delete tree in parallel
void
ParallelKDtree<point>::delete_tree_recursive( node* T ) {
  if ( T == nullptr ) return;
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
template class ParallelKDtree<PointType<long, 2>>;
template class ParallelKDtree<PointType<long, 3>>;
template class ParallelKDtree<PointType<long, 5>>;
template class ParallelKDtree<PointType<long, 7>>;
template class ParallelKDtree<PointType<long, 9>>;
template class ParallelKDtree<PointType<long, 10>>;
