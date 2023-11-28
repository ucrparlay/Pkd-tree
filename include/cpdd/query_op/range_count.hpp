#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

//* range count
template<typename point>
size_t
ParallelKDtree<point>::range_count(
    const typename ParallelKDtree<point>::box& bx, size_t& visNodeNum ) {
  return range_count_rectangle( this->root, bx, this->bbox, visNodeNum );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count_rectangle( node* T, const box& queryBox,
                                              const box& nodeBox,
                                              size_t& visNodeNum ) {
  visNodeNum++;

  if ( !box_intersect_box( nodeBox, queryBox ) ) return 0;
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
      false,
      [&] {
        l = range_count_rectangle( TI->left, queryBox, lbox, visNodeNum );
      },
      [&] {
        r = range_count_rectangle( TI->right, queryBox, rbox, visNodeNum );
      } );

  return std::move( l + r );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count( const circle& cl ) {
  return range_count_radius( this->root, cl, this->bbox );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count_radius( node* T, const circle& cl,
                                           const box& nodeBox ) {
  if ( !circle_intersect_box( cl, nodeBox ) ) return 0;
  if ( within_circle( nodeBox, cl ) ) return T->size;

  if ( T->is_leaf ) {
    size_t cnt = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_circle( TL->pts[( !T->is_dummy ) * i], cl ) ) {
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
      TI->size >= SERIAL_BUILD_CUTOFF,
      [&] { l = range_count_radius( TI->left, cl, lbox ); },
      [&] { r = range_count_radius( TI->right, cl, rbox ); } );

  return std::move( l + r );
};

template<typename point>
ParallelKDtree<point>::simple_node*
ParallelKDtree<point>::range_count_save_path( node* T, const box& queryBox,
                                              const box& nodeBox ) {
  if ( !intersect_box( nodeBox, queryBox ) ) return alloc_simple_node( 0 );
  if ( within_box( nodeBox, queryBox ) ) {
    return alloc_simple_node( T->size );
  }

  if ( T->is_leaf ) {
    size_t cnt = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        cnt++;
      }
    }
    return alloc_simple_node( cnt );
  }

  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  simple_node *L, *R;
  parlay::par_do_if(
      TI->size >= SERIAL_BUILD_CUTOFF,
      [&] { L = range_count_save_path( TI->left, queryBox, lbox ); },
      [&] { R = range_count_save_path( TI->right, queryBox, rbox ); } );

  return alloc_simple_node( L, R );
}

}  // namespace cpdd
