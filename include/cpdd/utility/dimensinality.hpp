#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {
template<typename point>
inline uint_fast8_t
ParallelKDtree<point>::pick_rebuild_dim( const node* T, const uint_fast8_t DIM ) {
  if ( this->_split_rule == MAX_STRETCH_DIM ) {
    return 0;
  } else if ( this->_split_rule == ROTATE_DIM ) {
    if ( T == this->root ) {
      return 0;
    } else {
      assert( !( T->parent->is_leaf ) );
      return ( static_cast<interior*>( T->parent )->split.second + 1 ) % DIM;
    }
  }
}

template<typename point>
inline uint_fast8_t
ParallelKDtree<point>::pick_max_stretch_dim( const box& bx, const uint_fast8_t DIM ) {
  uint_fast8_t d( 0 );
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