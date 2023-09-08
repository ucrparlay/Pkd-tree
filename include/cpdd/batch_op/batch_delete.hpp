#pragma once

#include "../kdTreeParallel.h"
#include "inner_tree.hpp"

namespace cpdd {

template<typename point>
void
ParallelKDtree<point>::batchDelete( slice A, const dim_type DIM ) {
  points B = points::uninitialized( A.size() );
  node* T = this->root;
  std::tie( this->root, this->bbox ) =
      batchDelete_recursive( T, A, parlay::make_slice( B ), DIM, 1 );
  return;
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::rebuild_after_delete( node* T, const dim_type DIM ) {
  points wo = points::uninitialized( T->size );
  points wx = points::uninitialized( T->size );
  uint_fast8_t d = pick_rebuild_dim( T, DIM );
  flatten( T, wx.cut( 0, T->size ) );
  delete_tree_recursive( T );
  box bx = get_box( parlay::make_slice( wx ) );
  node* o =
      build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM, bx );
  return node_box( std::move( o ), std::move( bx ) );
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::delete_inner_tree( bucket_type idx, const node_tags& tags,
                                          parlay::sequence<node_box>& treeNodes,
                                          bucket_type& p, const tag_nodes& rev_tag,
                                          const dim_type DIM ) {
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
      delete_tree_recursive( tags[idx].first );
      return node_box( alloc_leaf_node( points().cut( 0, 0 ) ), get_empty_box() );
    }
    return rebuild_after_delete( tags[idx].first, DIM );
  }

  return node_box( tags[idx].first, get_box( Lbox, Rbox ) );
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::batchDelete_recursive( node* T, slice In, slice Out,
                                              const dim_type DIM, bool hasTomb ) {
  size_t n = In.size();

  if ( n == 0 ) return node_box( T, get_box( T ) );

  if ( n == T->size ) {
    if ( hasTomb ) {
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
      return Num::Lt( p.pnt[TI->split.second], TI->split.first );
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
      return std::move( rebuild_after_delete( T, DIM ) );
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

  bucket_type beatles = 0;
  return delete_inner_tree( 1, IT.tags, treeNodes, beatles, IT.rev_tag, DIM );
}

}  // namespace cpdd