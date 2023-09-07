#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
struct ParallelKDtree<point>::InnerTree {
  //@ helpers
  bool
  assert_size( node* T ) const {
    if ( T->is_leaf ) {
      leaf* TI = static_cast<leaf*>( T );
      assert( T->size == TI->pts.size() && T->size <= LEAVE_WRAP );
      return true;
    }
    interior* TI = static_cast<interior*>( T );
    assert( TI->size == TI->left->size + TI->right->size );
    return true;
  }

  void
  assert_size_by_idx( int idx ) const {
    if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return;
    interior* TI = static_cast<interior*>( tags[idx].first );
    assert( TI->size == TI->left->size + TI->right->size );
    return;
  }

  inline int
  get_node_idx( int idx, node* T ) {
    if ( tags[idx].first == T ) return idx;
    if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return -1;
    auto pos = get_node_idx( idx << 1, T );
    if ( pos != -1 ) return pos;
    return get_node_idx( idx << 1 | 1, T );
  }

  //@ cores
  inline void
  reset_tags_num() {
    tagsNum = 0;
  }

  void
  assign_node_tag( node* T, int idx ) {
    if ( T->is_leaf || idx > PIVOT_NUM ) {
      assert( tagsNum < BUCKET_NUM );
      tags[idx] = node_tag( T, tagsNum++ );
      return;
    }
    //* BUCKET ID in [0, BUCKET_NUM)
    tags[idx] = node_tag( T, BUCKET_NUM );
    interior* TI = static_cast<interior*>( T );
    assign_node_tag( TI->left, idx << 1 );
    assign_node_tag( TI->right, idx << 1 | 1 );
    return;
  }

  void
  reduce_sums( int idx ) const {
    if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
      assert( tags[idx].second < BUCKET_NUM );
      sums_tree[idx] = sums[tags[idx].second];
      return;
    }
    reduce_sums( idx << 1 );
    reduce_sums( idx << 1 | 1 );
    sums_tree[idx] = sums_tree[idx << 1] + sums_tree[idx << 1 | 1];
    return;
  }

  void
  pick_tag( int idx ) {
    if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
      tags[idx].second = BUCKET_NUM + 1;
      rev_tag[tagsNum++] = idx;
      return;
    }
    assert( tags[idx].second == BUCKET_NUM && ( !tags[idx].first->is_leaf ) );
    interior* TI = static_cast<interior*>( tags[idx].first );

    if ( inbalance_node( TI->left->size + sums_tree[idx << 1],
                         TI->size + sums_tree[idx] ) ) {
      tags[idx].second = BUCKET_NUM + 2;
      rev_tag[tagsNum++] = idx;
      return;
    }
    pick_tag( idx << 1 );
    pick_tag( idx << 1 | 1 );
    return;
  }

  void
  tag_inbalance_node() {
    reduce_sums( 1 );
    reset_tags_num();
    pick_tag( 1 );
    assert( assert_size( tags[1].first ) );
    return;
  }

  //* the node which needs to be rebuilt has tag BUCKET_NUM+3
  //* the node whose ancestor has been rebuilt has tag BUCKET_NUM+2
  //* otherwise it has tag BUCKET_NUM+1
  void
  mark_tomb( int idx, bool hasTomb ) {
    if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
      assert( tags[idx].second >= 0 && tags[idx].second < BUCKET_NUM );
      tags[idx].second = ( !hasTomb ) ? BUCKET_NUM + 2 : BUCKET_NUM + 1;
      rev_tag[tagsNum++] = idx;
      return;
    }
    assert( tags[idx].second == BUCKET_NUM && ( !tags[idx].first->is_leaf ) );
    interior* TI = static_cast<interior*>( tags[idx].first );
    if ( hasTomb && ( inbalance_node( TI->left->size - sums_tree[idx << 1],
                                      TI->size - sums_tree[idx] ) ||
                      ( TI->size - sums_tree[idx] < THIN_LEAVE_WRAP ) ) ) {
      assert( hasTomb != 0 );
      tags[idx].second = BUCKET_NUM + 3;
      hasTomb = false;
    }

    mark_tomb( idx << 1, hasTomb );
    mark_tomb( idx << 1 | 1, hasTomb );
    return;
  }

  void
  tag_inbalance_node_deletion( bool hasTomb ) {
    reduce_sums( 1 );
    reset_tags_num();
    mark_tomb( 1, hasTomb );
    assert( assert_size( tags[1].first ) );
    return;
  }

  void
  init() {
    reset_tags_num();
    tags = node_tags::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
    sums_tree = parlay::sequence<uint_fast32_t>( PIVOT_NUM + BUCKET_NUM + 1 );
    rev_tag = tag_nodes::uninitialized( BUCKET_NUM );
  }

  //@ variables
  node_tags tags;
  parlay::sequence<uint_fast32_t> sums;
  mutable parlay::sequence<uint_fast32_t> sums_tree;
  mutable tag_nodes rev_tag;
  int tagsNum;
};
}  // namespace cpdd