#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
inline bool
ParallelKDtree<point>::legal_box( const box& bx ) {
  if ( bx == get_empty_box() ) return true;
  for ( int i = 0; i < bx.first.get_dim(); i++ ) {
    if ( Num::Gt( bx.first.pnt[i], bx.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::within_box( const box& a, const box& b ) {
  if ( !legal_box( a ) ) {
    LOG << a.first << " " << a.second << ENDL;
  }
  assert( legal_box( a ) );
  assert( legal_box( b ) );
  for ( int i = 0; i < a.first.get_dim(); i++ ) {
    if ( Num::Lt( a.first.pnt[i], b.first.pnt[i] ) ||
         Num::Gt( a.second.pnt[i], b.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::within_box( const point& p, const box& bx ) {
  assert( legal_box( bx ) );
  for ( int i = 0; i < p.get_dim(); i++ ) {
    if ( Num::Lt( p.pnt[i], bx.first.pnt[i] ) || Num::Gt( p.pnt[i], bx.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::intersect_box( const box& a, const box& b ) {
  assert( legal_box( a ) && legal_box( b ) );
  for ( int i = 0; i < a.first.get_dim(); i++ ) {
    if ( Num::Gt( a.first.pnt[i], b.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_empty_box() {
  return std::move( box( point( std::numeric_limits<coord>::max() ),
                         point( std::numeric_limits<coord>::min() ) ) );
}

template<typename point>
typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_box( const box& x, const box& y ) {
  return std::move( box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) ) );
}

template<typename point>
typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_box( slice V ) {
  if ( V.size() == 0 ) {
    return std::move( get_empty_box() );
  } else {
    auto minmax = [&]( const box& x, const box& y ) {
      return box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) );
    };
    auto boxes = parlay::delayed_seq<box>(
        V.size(), [&]( size_t i ) { return box( V[i].pnt, V[i].pnt ); } );
    return std::move( parlay::reduce( boxes, parlay::make_monoid( minmax, boxes[0] ) ) );
  }
}

template<typename point>
typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_box( node* T ) {
  points wx = points::uninitialized( T->size );
  flatten( T, parlay::make_slice( wx ) );
  return std::move( get_box( parlay::make_slice( wx ) ) );
}

}  // namespace cpdd