#include "kdTreeParallel.h"

//@ Find Bounding Box

//@ Support Functions
template<typename point>
inline typename ParallelKDtree<point>::coord
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
                                      const uint_fast8_t& DIM, box_s& boxs,
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
                              return Num.Lt( p1.pnt[d], p2.pnt[d] );
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

//@ starting at dimesion dim and pick pivots in a rotation manner
template<typename point>
void
ParallelKDtree<point>::pick_pivots( slice In, const size_t& n, splitter_s& pivots,
                                    const uint_fast8_t& dim, const uint_fast8_t& DIM,
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
    k = Num.Lt( p.pnt[pivots[k].second], pivots[k].first ) ? k << 1 : k << 1 | 1;
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
  return alloc_interior_node( L, R, pivots[idx] );
}

//@ Parallel KD tree cores
template<typename point>
void
ParallelKDtree<point>::build( slice A, const uint_fast8_t& DIM ) {
  points B = points::uninitialized( A.size() );
  this->bbox = get_box( A );
  // LOG << bx.first << " " << bx.second << ENDL;
  this->root = build_recursive( A, B.cut( 0, A.size() ), 0, DIM, this->bbox );
  assert( this->root != NULL );
  return;
}

template<typename point>
NODE<point>*
ParallelKDtree<point>::build_recursive( slice In, slice Out, uint_fast8_t dim,
                                        const uint_fast8_t& DIM, box bx ) {
  assert( within_box( get_box( In ), bx ) );
  size_t n = In.size();

  if ( n <= LEAVE_WRAP ) {
    return alloc_leaf_node( In );
  }

  //* serial run nth element
  if ( n <= SERIAL_BUILD_CUTOFF ) {
    uint_fast8_t d =
        ( _split_rule == MAX_STRETCH_DIM ? pick_max_stretch_dim( bx, DIM ) : dim );
    assert( d >= 0 && d < DIM );

    std::ranges::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                              [&]( const point& p1, const point& p2 ) {
                                return Num.Lt( p1.pnt[d], p2.pnt[d] );
                              } );
    splitter split = splitter( In[n / 2].pnt[d], d );

    auto _2ndGroup = std::ranges::partition(
        In.begin(), In.begin() + n / 2,
        [&]( const point& p ) { return Num.Lt( p.pnt[split.second], split.first ); } );

    if ( _2ndGroup.begin() == In.begin() ) {
      // LOG << "sort right" << ENDL;
      assert( std::ranges::all_of( In.begin() + n / 2, In.end(),
                                   [&]( const point& p ) {
                                     return Num.Geq( p.pnt[split.second], split.first );
                                   } ) &&
              std::ranges::all_of( In.begin(), In.begin() + n / 2, [&]( const point& p ) {
                return Num.Eq( p.pnt[split.second], split.first );
              } ) );

      _2ndGroup = std::ranges::partition(
          In.begin() + n / 2, In.end(),
          [&]( const point& p ) { return Num.Eq( p.pnt[split.second], split.first ); } );
      assert( _2ndGroup.begin() - ( In.begin() + n / 2 ) > 0 );
      points_iter diffEleIter;

      if ( _2ndGroup.begin() != In.end() ) {
        //* need to change split
        auto minEleIter =
            std::ranges::min_element( _2ndGroup, [&]( const point& p1, const point& p2 ) {
              return Num.Lt( p1.pnt[split.second], p2.pnt[split.second] );
            } );
        assert( minEleIter != In.end() );
        split.first = minEleIter->pnt[split.second];
        assert( std::ranges::all_of( In.begin(), _2ndGroup.begin(),
                                     [&]( const point& p ) {
                                       return Num.Lt( p.pnt[split.second], split.first );
                                     } ) &&
                std::ranges::all_of( _2ndGroup, [&]( const point& p ) {
                  return Num.Geq( p.pnt[split.second], split.first );
                } ) );
      } else if ( In.end() == ( diffEleIter = std::ranges::find_if_not(
                                    In.begin(), In.end(), [&]( const point& p ) {
                                      return p == In[n / 2];
                                    } ) ) ) {
        assert( _2ndGroup.begin() == In.end() && diffEleIter == In.end() );
        return alloc_dummy_leaf( In );
      } else {  //* current dim d is same but other dims are not
        if ( _split_rule == MAX_STRETCH_DIM ) {  //* next recursion redirects to new dim
          return build_recursive( In, Out, d, DIM, get_box( In ) );
        } else if ( _split_rule == ROTATE_DIM ) {  //* switch dim, break rotation order
          decltype( diffEleIter ) compIter =
              diffEleIter == In.begin() ? In.begin() + n - 1 : In.begin();
          assert( compIter != diffEleIter );
          for ( int i = 0; i < DIM; i++ ) {
            // if ( diffEleIter->pnt[i] != compIter->pnt[i] ) {
            if ( !Num.Eq( diffEleIter->pnt[i], compIter->pnt[i] ) ) {
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
    L = build_recursive( In.cut( 0, _2ndGroup.begin() - In.begin() ),
                         Out.cut( 0, _2ndGroup.begin() - In.begin() ), d, DIM, lbox );
    R = build_recursive( In.cut( _2ndGroup.begin() - In.begin(), n ),
                         Out.cut( _2ndGroup.begin() - In.begin(), n ), d, DIM, rbox );
    return alloc_interior_node( L, R, split );
  }

  //* parallel partitons
  auto pivots = splitter_s::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
  auto boxs = box_s::uninitialized( BUCKET_NUM );
  parlay::sequence<uint_fast32_t> sums;

  pick_pivots( In, n, pivots, dim, DIM, boxs, bx );
  partition( In, Out, n, pivots, sums );
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

template<typename point>
void
ParallelKDtree<point>::flatten( NODE<point>* T, slice Out ) {
  assert( T->size == Out.size() );
  if ( T->size == 0 ) return;

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      Out[i] = TL->pts[( !T->is_dummy ) * i];
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
      Out[i] = TL->pts[( !T->is_dummy ) * i];
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
  L->parent = T;
  R->parent = T;
  return;
}

template<typename point>
uint_fast8_t
ParallelKDtree<point>::retrive_tag( const point& p, const node_tags& tags ) {
  uint_fast8_t k = 1;
  interior* TI;

  while ( k <= PIVOT_NUM && ( !tags[k].first->is_leaf ) ) {
    TI = static_cast<interior*>( tags[k].first );
    // k = p.pnt[TI->split.second] < TI->split.first ? k << 1 : k << 1 | 1;
    k = Num.Lt( p.pnt[TI->split.second], TI->split.first ) ? k << 1 : k << 1 | 1;
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
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      offset[i][std::move( retrive_tag( A[j], tags ) )]++;
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
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      B[v[std::move( retrive_tag( A[j], tags ) )]++] = A[j];
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
    if ( !T->is_dummy && n + TL->size <= LEAVE_WRAP ) {
      assert( T->size == TL->size );
      for ( int i = 0; i < n; i++ ) {
        TL->pts[TL->size + i] = In[i];
      }
      TL->size += n;
      return T;
    } else {
      points wx = points::uninitialized( n + TL->size );
      points wo = points::uninitialized( n + TL->size );
      assert( !T->is_dummy || T->size >= LEAVE_WRAP );
      for ( int i = 0; i < TL->size; i++ ) {
        wx[i] = TL->pts[( !T->is_dummy ) * i];
      }
      parlay::parallel_for(
          0, n, [&]( size_t i ) { wx[TL->size + i] = In[i]; }, BLOCK_SIZE );
      uint_fast8_t d = pick_rebuild_dim( T, DIM );
      free_leaf( T );
      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM,
                              get_box( parlay::make_slice( wx ) ) );
    }
  }

  if ( n <= SERIAL_BUILD_CUTOFF ) {
    interior* TI = static_cast<interior*>( T );
    auto _2ndGroup = std::ranges::partition( In, [&]( const point& p ) {
      return Num.Lt( p.pnt[TI->split.second], TI->split.first );
    } );

    //* rebuild
    if ( inbalance_node( TI->left->size + _2ndGroup.begin() - In.begin(),
                         TI->size + n ) ) {
      points wx = points::uninitialized( T->size + In.size() );
      points wo = points::uninitialized( T->size + In.size() );
      // uint_fast8_t d = T->dim;
      uint_fast8_t d = pick_rebuild_dim( T, DIM );
      flatten( T, wx.cut( 0, T->size ) );
      parlay::parallel_for(
          0, n, [&]( size_t j ) { wx[T->size + j] = In[j]; }, BLOCK_SIZE );
      delete_tree_recursive( T );
      return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM,
                              get_box( parlay::make_slice( wx ) ) );
    }
    //* continue
    node *L, *R;
    L = batchInsert_recusive( TI->left, In.cut( 0, _2ndGroup.begin() - In.begin() ),
                              Out.cut( 0, _2ndGroup.begin() - In.begin() ), DIM );
    R = batchInsert_recusive( TI->right, In.cut( _2ndGroup.begin() - In.begin(), n ),
                              Out.cut( _2ndGroup.begin() - In.begin(), n ), DIM );
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
          uint_fast8_t d = pick_rebuild_dim( IT.tags[IT.rev_tag[i]].first, DIM );
          // uint_fast8_t d = IT.tags[IT.rev_tag[i]].first->dim;
          size_t head_size = IT.tags[IT.rev_tag[i]].first->size;

          flatten( IT.tags[IT.rev_tag[i]].first,
                   wx.cut( 0, IT.tags[IT.rev_tag[i]].first->size ) );
          delete_tree_recursive( IT.tags[IT.rev_tag[i]].first );

          parlay::parallel_for(
              0, IT.sums_tree[IT.rev_tag[i]],
              [&]( size_t j ) { wx[head_size + j] = Out[s + j]; }, BLOCK_SIZE );

          treeNodes[i] =
              build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM,
                               get_box( parlay::make_slice( wx ) ) );
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
  box b = get_box( A );
  this->bbox = get_box( this->bbox, get_box( A ) );
  this->root = batchInsert_recusive( T, A, B.cut( 0, A.size() ), DIM );
  assert( this->root != NULL );
  return;
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::delete_inner_tree( uint_fast32_t idx, const node_tags& tags,
                                          parlay::sequence<node_box>& treeNodes, int& p,
                                          const tag_nodes& rev_tag,
                                          const uint_fast8_t& DIM ) {
  if ( tags[idx].second == BUCKET_NUM + 1 || tags[idx].second == BUCKET_NUM + 2 ) {
    assert( rev_tag[p] == idx );
    return treeNodes[p++];
  }

  auto [L, Lbox] = delete_inner_tree( idx << 1, tags, treeNodes, p, rev_tag, DIM );
  auto [R, Rbox] = delete_inner_tree( idx << 1 | 1, tags, treeNodes, p, rev_tag, DIM );
  update_interior( tags[idx].first, L, R );

  if ( tags[idx].second == BUCKET_NUM + 3 ) {
    interior* TI = static_cast<interior*>( tags[idx].first );
    assert( inbalance_node( TI->left->size, TI->size ) || TI->size < THIN_LEAVE_WRAP );
    if ( tags[idx].first->size == 0 ) {  //* special judge for empty tree
      // uint_fast8_t d = tags[idx].first->dim;
      // uint_fast8_t d = pick_rebuild_dim( tags[idx].first, DIM );
      delete_tree_recursive( tags[idx].first );
      return node_box( alloc_leaf_node( points().cut( 0, 0 ) ), get_empty_box() );
    }
    points wx = points::uninitialized( tags[idx].first->size );
    points wo = points::uninitialized( tags[idx].first->size );
    uint_fast8_t d = pick_rebuild_dim( tags[idx].first, DIM );
    flatten( tags[idx].first, wx.cut( 0, tags[idx].first->size ) );
    delete_tree_recursive( tags[idx].first );
    box bx = get_box( parlay::make_slice( wx ) );
    node* o =
        build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM, bx );
    return node_box( o, bx );
  }

  return node_box( tags[idx].first, get_box( Lbox, Rbox ) );
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::batchDelete_recursive( node* T, slice In, slice Out,
                                              const uint_fast8_t& DIM, bool hasTomb ) {
  size_t n = In.size();

  if ( n == 0 ) return node_box( T, get_box( T ) );

  if ( n == T->size ) {
    if ( hasTomb ) {
      // uint_fast8_t d = pick_rebuild_dim( T, DIM );
      delete_tree_recursive( T );
      return node_box( alloc_leaf_node( In.cut( 0, 0 ) ), get_empty_box() );
    }
    T->size = 0;  //* lazy mark
    return node_box( T, get_empty_box() );
  }

  if ( T->is_dummy ) {
    assert( T->is_leaf );
    assert( In.size() <= T->size );
    leaf* TL = static_cast<leaf*>( T );
    T->size -= In.size();
    return node_box( T, box( TL->pts[0], TL->pts[0] ) );
  }

  if ( T->is_leaf ) {
    assert( !T->is_dummy );
    assert( T->size >= In.size() );

    leaf* TL = static_cast<leaf*>( T );
    auto it = TL->pts.begin(), end = TL->pts.begin() + TL->size;
    for ( int i = 0; i < In.size(); i++ ) {
      it = std::ranges::find( TL->pts.begin(), end, In[i] );
      assert( it != end );
      std::ranges::iter_swap( it, --end );
    }

    assert( std::distance( TL->pts.begin(), end ) == TL->size - In.size() );
    TL->size -= In.size();
    assert( TL->size >= 0 );
    return node_box( T, get_box( TL->pts.cut( 0, TL->size ) ) );
  }

  if ( In.size() <= SERIAL_BUILD_CUTOFF ) {
    interior* TI = static_cast<interior*>( T );
    auto _2ndGroup = std::ranges::partition( In, [&]( const point& p ) {
      return Num.Lt( p.pnt[TI->split.second], TI->split.first );
    } );

    bool putTomb =
        hasTomb && ( inbalance_node( TI->left->size - ( _2ndGroup.begin() - In.begin() ),
                                     TI->size - In.size() ) ||
                     TI->size - In.size() < THIN_LEAVE_WRAP );
    hasTomb = putTomb ? false : hasTomb;
    assert( putTomb ? ( !hasTomb ) : true );

    auto [L, Lbox] = batchDelete_recursive(
        TI->left, In.cut( 0, _2ndGroup.begin() - In.begin() ),
        Out.cut( 0, _2ndGroup.begin() - In.begin() ), DIM, hasTomb );
    auto [R, Rbox] = batchDelete_recursive(
        TI->right, In.cut( _2ndGroup.begin() - In.begin(), n ),
        Out.cut( _2ndGroup.begin() - In.begin(), n ), DIM, hasTomb );
    update_interior( T, L, R );
    assert( T->size == L->size + R->size && TI->split.second >= 0 &&
            TI->is_leaf == false );

    //* rebuild
    if ( putTomb ) {
      assert( TI->size == T->size );
      assert( inbalance_node( TI->left->size, TI->size ) || TI->size < THIN_LEAVE_WRAP );
      points wx = points::uninitialized( T->size );
      points wo = points::uninitialized( T->size );
      uint_fast8_t d = pick_rebuild_dim( T, DIM );
      // uint_fast8_t d = T->dim;
      flatten( T, wx.cut( 0, T->size ) );
      delete_tree_recursive( T );
      box bx = get_box( parlay::make_slice( wx ) );
      node* o = build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d,
                                 DIM, bx );
      return node_box( o, bx );
    }

    return node_box( T, get_box( Lbox, Rbox ) );
  }

  InnerTree IT;
  IT.init();
  IT.assign_node_tag( T, 1 );
  assert( IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM );
  seieve_points( In, Out, n, IT.tags, IT.sums, IT.tagsNum );
  IT.tag_inbalance_node_deletion( hasTomb );

  auto treeNodes = parlay::sequence<node_box>::uninitialized( IT.tagsNum );
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
  std::tie( this->root, this->bbox ) =
      batchDelete_recursive( T, A, parlay::make_slice( B ), DIM, 1 );

  return;
}

//? parallel query
template<typename point>
void
ParallelKDtree<point>::k_nearest( node* T, const point& q, const uint_fast8_t& DIM,
                                  kBoundedQueue<point>& bq, size_t& visNodeNum ) {
  visNodeNum++;

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      bq.insert(
          std::make_pair( TL->pts[( !T->is_dummy ) * i],
                          ppDistanceSquared( q, TL->pts[( !T->is_dummy ) * i], DIM ) ) );
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  auto dx = [&]() { return TI->split.first - q.pnt[TI->split.second]; };

  k_nearest( Num.Gt( dx(), 0 ) ? TI->left : TI->right, q, DIM, bq, visNodeNum );
  if ( Num.Gt( dx() * dx(), bq.top().second ) && bq.full() ) {
    return;
  }
  k_nearest( Num.Gt( dx(), 0 ) ? TI->right : TI->left, q, DIM, bq, visNodeNum );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count( const typename ParallelKDtree<point>::box& bx ) {
  return range_count_value( this->root, bx, this->bbox );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count_value( node* T, const box& queryBox,
                                          const box& nodeBox ) {
  if ( !intersect_box( nodeBox, queryBox ) ) return 0;
  if ( within_box( nodeBox, queryBox ) ) return T->size;

  if ( T->is_leaf ) {
    size_t cnt = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        cnt++;
      }
    }
    return std::move( cnt );
  }

  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  size_t l, r;
  parlay::par_do_if(
      // TI->size >= SERIAL_BUILD_CUTOFF,
      0, [&] { l = range_count_value( TI->left, queryBox, lbox ); },
      [&] { r = range_count_value( TI->right, queryBox, rbox ); } );

  return std::move( l + r );
}

template<typename point>
void
ParallelKDtree<point>::range_count_recursive( node* T, const box& queryBox,
                                              const box& nodeBox ) {
  if ( !intersect_box( nodeBox, queryBox ) ) {
    T->aug = 0;
    return;
  }

  if ( within_box( nodeBox, queryBox ) ) {
    T->aug = T->size;
    return;
  }

  if ( T->is_leaf ) {
    T->aug = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        T->aug++;
      }
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  parlay::par_do_if(
      TI->size >= SERIAL_BUILD_CUTOFF,
      [&] { range_count_recursive( TI->left, queryBox, lbox ); },
      [&] { range_count_recursive( TI->right, queryBox, rbox ); } );

  TI->aug = TI->left->aug + TI->right->aug;

  return;
}

template<typename point>
size_t
ParallelKDtree<point>::range_query( const typename ParallelKDtree<point>::box& bx,
                                    slice Out ) {
  // range_count_recursive( this->root, bx, this->bbox );
  // size_t n = this->root->aug;

  // todo no need to calculate range n below
  // size_t n = range_count_value( this->root, bx, this->bbox );
  // if ( Out.size() < n ) {
  //   throw( "too small output size for range query" );
  //   abort();
  // }
  // assert( Out.size() >= n );
  size_t s = 0;
  range_query_recursive( this->root, Out, s, bx, this->bbox );
  return s;
}

template<typename point>
void
ParallelKDtree<point>::range_query_recursive( node* T, slice Out, size_t& s,
                                              const box& queryBox, const box& nodeBox ) {
  if ( !intersect_box( nodeBox, queryBox ) ) {
    return;
  }

  if ( within_box( nodeBox, queryBox ) ) {
    // assert( T->size == Out.size() );
    // flatten( T, Out );
    flatten( T, Out.cut( s, s + T->size ) );
    s += T->size;
    return;
  }

  if ( T->is_leaf ) {
    // size_t cnt = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        // Out[cnt++] = TL->pts[i];
        Out[s++] = TL->pts[( !T->is_dummy ) * i];
      }
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  // assert( TI->left->aug + TI->right->aug == Out.size() );

  parlay::par_do_if(
      // TI->size >= SERIAL_BUILD_CUTOFF,
      0, [&] { range_query_recursive( TI->left, Out, s, queryBox, lbox ); },
      [&] { range_query_recursive( TI->right, Out, s, queryBox, rbox ); } );

  return;
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
template class Comparator<long>;
template class Comparator<double>;

template class ParallelKDtree<PointType<long, 2>>;
template class ParallelKDtree<PointType<long, 3>>;
template class ParallelKDtree<PointType<long, 5>>;
template class ParallelKDtree<PointType<long, 7>>;
template class ParallelKDtree<PointType<long, 9>>;
template class ParallelKDtree<PointType<long, 10>>;

template class ParallelKDtree<PointType<double, 2>>;
template class ParallelKDtree<PointType<double, 3>>;
template class ParallelKDtree<PointType<double, 5>>;
template class ParallelKDtree<PointType<double, 7>>;
template class ParallelKDtree<PointType<double, 9>>;
template class ParallelKDtree<PointType<double, 10>>;
