#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
template<typename Slice>
void
ParallelKDtree<point>::flatten( typename ParallelKDtree<point>::node* T, Slice Out ) {
  // if ( T->size != Out.size() ) {
  //   LOG << T->size << " " << Out.size() << ENDL;
  // }
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
ParallelKDtree<point>::flatten_and_delete( typename ParallelKDtree<point>::node* T,
                                           slice Out ) {
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
ParallelKDtree<point>::update_interior( typename ParallelKDtree<point>::node* T,
                                        typename ParallelKDtree<point>::node* L,
                                        typename ParallelKDtree<point>::node* R ) {
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
    k = Num::Lt( p.pnt[TI->split.second], TI->split.first ) ? k << 1 : k << 1 | 1;
  }
  assert( tags[k].second < BUCKET_NUM );
  return tags[k].second;
}

template<typename point>
void
ParallelKDtree<point>::seieve_points( slice A, slice B, const size_t n,
                                      const node_tags& tags,
                                      parlay::sequence<balls_type>& sums,
                                      const bucket_type tagsNum ) {
  size_t num_block = ( n + BLOCK_SIZE - 1 ) >> LOG2_BASE;
  parlay::sequence<parlay::sequence<balls_type>> offset(
      num_block, parlay::sequence<balls_type>( tagsNum ) );
  assert( offset.size() == num_block && offset[0].size() == tagsNum &&
          offset[0][0] == 0 );
  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    for ( size_t j = i << LOG2_BASE; j < std::min( ( i + 1 ) << LOG2_BASE, n ); j++ ) {
      offset[i][std::move( retrive_tag( A[j], tags ) )]++;
    }
  } );

  sums = parlay::sequence<balls_type>( tagsNum );
  for ( size_t i = 0; i < num_block; i++ ) {
    auto t = offset[i];
    offset[i] = sums;
    for ( int j = 0; j < tagsNum; j++ ) {
      sums[j] += t[j];
    }
  }

  parlay::parallel_for( 0, num_block, [&]( size_t i ) {
    auto v = parlay::sequence<balls_type>::uninitialized( tagsNum );
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
typename ParallelKDtree<point>::node*
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

}  // namespace cpdd