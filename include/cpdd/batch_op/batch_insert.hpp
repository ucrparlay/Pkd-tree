#pragma once

#include "../kdTreeParallel.h"
#include "inner_tree.hpp"

namespace cpdd {

template<typename point>
void
ParallelKDtree<point>::batchInsert( slice A, const dim_type DIM ) {
  points B = points::uninitialized( A.size() );
  node* T = this->root;
  box b = get_box( A );
  this->bbox = get_box( this->bbox, get_box( A ) );
  this->root = batchInsert_recusive( T, A, B.cut( 0, A.size() ), DIM );
  assert( this->root != NULL );
  return;
}

template<typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::update_inner_tree( bucket_type idx, const node_tags& tags,
                                          parlay::sequence<node*>& treeNodes,
                                          bucket_type& p, const tag_nodes& rev_tag ) {

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

template<typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::rebuild_with_insert( node* T, slice In, const dim_type DIM ) {
  uint_fast8_t d = pick_rebuild_dim( T, DIM );
  points wo = points::uninitialized( T->size + In.size() );
  points wx = points::uninitialized( T->size + In.size() );
  parlay::parallel_for( 0, In.size(), [&]( size_t j ) { wx[j] = In[j]; } );
  flatten( T, wx.cut( In.size(), wx.size() ) );
  delete_tree_recursive( T );
  return build_recursive( parlay::make_slice( wx ), parlay::make_slice( wo ), d, DIM,
                          get_box( parlay::make_slice( wx ) ) );
}

//* return the updated node
template<typename point>
typename ParallelKDtree<point>::node*
ParallelKDtree<point>::batchInsert_recusive( node* T, slice In, slice Out,
                                             const dim_type DIM ) {
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
      return rebuild_with_insert( T, In, DIM );
    }
  }

  if ( n <= SERIAL_BUILD_CUTOFF ) {
    interior* TI = static_cast<interior*>( T );
    auto _2ndGroup = std::ranges::partition( In, [&]( const point& p ) {
      return Num::Lt( p.pnt[TI->split.second], TI->split.first );
    } );

    //* rebuild
    if ( inbalance_node( TI->left->size + _2ndGroup.begin() - In.begin(),
                         TI->size + n ) ) {
      return rebuild_with_insert( T, In, DIM );
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
          treeNodes[i] =
              rebuild_with_insert( IT.tags[IT.rev_tag[i]].first,
                                   Out.cut( s, s + IT.sums_tree[IT.rev_tag[i]] ), DIM );
        }
      },
      1 );

  bucket_type beatles = 0;
  return update_inner_tree( 1, IT.tags, treeNodes, beatles, IT.rev_tag );
}

}  // namespace cpdd