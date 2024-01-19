#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

/*TODO range count */
template<typename point>
size_t
ParallelKDtree<point>::range_count( const typename ParallelKDtree<point>::box& bx,
                                    size_t& visLeafNum, size_t& visInterNum ) {
  return range_count_rectangle( this->root, bx, this->bbox, visLeafNum, visInterNum );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count_rectangle( node* T, const box& queryBox,
                                              const box& nodeBox, size_t& visLeafNum,
                                              size_t& visInterNum ) {

  if ( T->is_leaf ) {
    visLeafNum++;
    size_t cnt = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        cnt++;
      }
    }
    return cnt;
  }

  visInterNum++;
  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  // TODO loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  size_t l, r;

  auto recurse = [&]( node* Ts, const box& bx, size_t& conter ) {
    if ( !box_intersect_box( bx, queryBox ) ) {
      conter = 0;
    } else if ( within_box( bx, queryBox ) ) {
      conter = Ts->size;
    } else {
      conter = range_count_rectangle( Ts, queryBox, bx, visLeafNum, visInterNum );
    }
  };

  recurse( TI->left, lbox, l );
  recurse( TI->right, rbox, r );

  return std::move( l + r );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count( const circle& cl ) {
  return range_count_radius( this->root, cl, this->bbox );
}

// TODO as range_count_rectangle
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
    return cnt;
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
