#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
inline bool
ParallelKDtree<point>::legal_box( const box& bx ) {
  if ( bx == get_empty_box() ) return true;
  for ( dim_type i = 0; i < bx.first.get_dim(); ++i ) {
    if ( Num::Gt( bx.first.pnt[i], bx.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::within_box( const box& a, const box& b ) {
  assert( legal_box( a ) );
  assert( legal_box( b ) );
  for ( dim_type i = 0; i < a.first.get_dim(); ++i ) {
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
  for ( dim_type i = 0; i < p.get_dim(); ++i ) {
    if ( Num::Lt( p.pnt[i], bx.first.pnt[i] ) || Num::Gt( p.pnt[i], bx.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::box_intersect_box( const box& a, const box& b ) {
  assert( legal_box( a ) && legal_box( b ) );
  for ( dim_type i = 0; i < a.first.get_dim(); ++i ) {
    if ( Num::Lt( a.second.pnt[i], b.first.pnt[i] ) ||
         Num::Gt( a.first.pnt[i], b.second.pnt[i] ) ) {
      return false;
    }
  }
  return true;
}

template<typename point>
inline typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_empty_box() {
  return box( point( std::numeric_limits<coord>::max() ),
              point( std::numeric_limits<coord>::min() ) );
}

template<typename point>
typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_box( const box& x, const box& y ) {
  return box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) );
}

template<typename point>
typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_box( slice V ) {
  if ( V.size() == 0 ) {
    return get_empty_box();
  } else {
    auto minmax = [&]( const box& x, const box& y ) {
      return box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) );
    };
    auto boxes = parlay::delayed_seq<box>(
        V.size(), [&]( size_t i ) { return box( V[i].pnt, V[i].pnt ); } );
    return parlay::reduce( boxes, parlay::make_monoid( minmax, boxes[0] ) );
  }
}

template<typename point>
typename ParallelKDtree<point>::box
ParallelKDtree<point>::get_box( node* T ) {
  points wx = points::uninitialized( T->size );
  flatten( T, parlay::make_slice( wx ) );
  return get_box( parlay::make_slice( wx ) );
}

template<typename point>
inline bool
ParallelKDtree<point>::within_circle( const box& bx, const circle& cl ) {
  //* the logical is same as p2b_max_distance <= radius
  coord r = 0;
  for ( dim_type i = 0; i < cl.first.get_dim(); ++i ) {
    if ( Num::Lt( cl.first.pnt[i], ( bx.first.pnt[i] + bx.second.pnt[i] ) / 2 ) ) {
      r +=
          ( bx.second.pnt[i] - cl.first.pnt[i] ) * ( bx.second.pnt[i] - cl.first.pnt[i] );
    } else {
      r += ( cl.first.pnt[i] - bx.first.pnt[i] ) * ( cl.first.pnt[i] - bx.first.pnt[i] );
    }
    if ( Num::Gt( r, cl.second * cl.second ) ) return false;
  }
  assert( Num::Leq( r, cl.second * cl.second ) );
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::within_circle( const point& p, const circle& cl ) {
  coord r = 0;
  for ( dim_type i = 0; i < cl.first.get_dim(); ++i ) {
    r += ( p.pnt[i] - cl.first.pnt[i] ) * ( p.pnt[i] - cl.first.pnt[i] );
    if ( Num::Gt( r, cl.second * cl.second ) ) return false;
  }
  assert( Num::Leq( r, cl.second * cl.second ) );
  return true;
}

template<typename point>
inline bool
ParallelKDtree<point>::circle_intersect_box( const circle& cl, const box& bx ) {
  //* the logical is same as p2b_min_distance > radius
  coord r = 0;
  for ( dim_type i = 0; i < cl.first.get_dim(); ++i ) {
    if ( Num::Lt( cl.first.pnt[i], bx.first.pnt[i] ) ) {
      r += ( bx.first.pnt[i] - cl.first.pnt[i] ) * ( bx.first.pnt[i] - cl.first.pnt[i] );
    } else if ( Num::Gt( cl.first.pnt[i], bx.second.pnt[i] ) ) {
      r +=
          ( cl.first.pnt[i] - bx.second.pnt[i] ) * ( cl.first.pnt[i] - bx.second.pnt[i] );
    }
    if ( Num::Gt( r, cl.second * cl.second ) ) return false;  //* not intersect
  }
  assert( Num::Leq( r, cl.second * cl.second ) );
  return true;
}

}  // namespace cpdd
