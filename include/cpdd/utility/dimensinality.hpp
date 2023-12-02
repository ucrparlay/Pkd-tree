#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {
template<typename point>
inline ParallelKDtree<point>::dim_type
ParallelKDtree<point>::pick_rebuild_dim( const node* T, const dim_type DIM ) {
  if ( this->_split_rule == MAX_STRETCH_DIM ) {
    return 0;
  } else if ( this->_split_rule == ROTATE_DIM ) {
    if ( T == this->root ) {
      return 0;
    } else {
      assert( !( T->parent->is_leaf ) );
      return ( static_cast<interior*>( T->parent )->split.second + 1 ) % DIM;
    }
  } else {
    // WARN: customize it before using
    return 0;
  }
}

template<typename point>
inline ParallelKDtree<point>::dim_type
ParallelKDtree<point>::pick_max_stretch_dim( const box& bx, const dim_type DIM ) {
  dim_type d( 0 );
  coord diff( bx.second.pnt[0] - bx.first.pnt[0] );
  assert( Num::Geq( diff, 0 ) );
  for ( int i = 1; i < DIM; i++ ) {
    if ( Num::Gt( bx.second.pnt[i] - bx.first.pnt[i], diff ) ) {
      diff = bx.second.pnt[i] - bx.first.pnt[i];
      d = i;
    }
  }
  return d;
}
}  // namespace cpdd
