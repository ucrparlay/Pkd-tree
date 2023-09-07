#pragma once

#include <cstdint>

#include "comparator.h"
#include "parlay_headers.h"

template<typename T, uint_fast8_t d>
struct PointType {
  using coord = T;
  using coords = std::array<T, d>;
  using Num = Num<coord>;

  PointType() {}

  PointType( const T val ) { pnt.fill( val ); }

  PointType( const coords& _pnt ) : pnt( _pnt ){};

  PointType( parlay::slice<T*, T*> x ) {
    assert( x.size() == d );
    for ( int i = 0; i < d; i++ )
      pnt[i] = x[i];
  }

  PointType( T* x ) {
    for ( int i = 0; i < d; i++ )
      pnt[i] = x[i];
  }

  const PointType
  minCoords( const PointType& b ) const {
    coords pts;
    for ( int i = 0; i < d; i++ ) {
      pts[i] = std::min( pnt[i], b.pnt[i] );
    }
    return std::move( pts );
  }

  const PointType
  maxCoords( const PointType& b ) const {
    coords pts;
    for ( int i = 0; i < d; i++ ) {
      pts[i] = std::max( pnt[i], b.pnt[i] );
    }
    return std::move( pts );
  }

  const uint_fast8_t
  get_dim() const {
    return std::move( pnt.size() );
  }

  bool
  operator==( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( !Num::Eq( pnt[i], x.pnt[i] ) ) return false;
    }
    return true;
  }

  bool
  operator<( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( Num::Lt( pnt[i], x.pnt[i] ) )
        return true;
      else if ( Num::Gt( pnt[i], x.pnt[i] ) )
        return false;
      else
        continue;
    }
    return false;
  }

  friend std::ostream&
  operator<<( std::ostream& o, PointType const& a ) {
    o << "(";
    for ( int i = 0; i < d; i++ ) {
      o << a.pnt[i] << ", ";
    }
    o << ") " << std::flush;
    return o;
  }

  coords pnt;
};