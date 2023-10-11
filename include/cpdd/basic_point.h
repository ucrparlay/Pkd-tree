#pragma once

#include "comparator.h"

#include "parlay/alloc.h"
#include "parlay/delayed.h"
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/utilities.h"

namespace cpdd {

template<typename T, uint_fast8_t d>
struct PointType {
  using coord = T;
  using coords = std::array<T, d>;
  using Num = Num_Comparator<coord>;

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

  inline const PointType
  minCoords( const PointType& b ) const {
    coords pts;
    for ( int i = 0; i < d; i++ ) {
      pts[i] = Num::min( pnt[i], b.pnt[i] );
    }
    return std::move( PointType( pts ) );
  }

  inline const PointType
  maxCoords( const PointType& b ) const {
    coords pts;
    for ( int i = 0; i < d; i++ ) {
      pts[i] = Num::max( pnt[i], b.pnt[i] );
    }
    return std::move( PointType( pts ) );
  }

  inline const uint_fast8_t
  get_dim() const {
    return std::move( pnt.size() );
  }

  inline bool
  sameDimension( const PointType& b ) const {
    return *this == b;
  }

  inline bool
  operator==( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( !Num::Eq( pnt[i], x.pnt[i] ) ) return false;
    }
    return true;
  }

  inline bool
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

template<typename T, uint_fast8_t d, typename IDtype = uint>
struct PointID : PointType<T, d> {
  using coord = T;
  using coords = std::array<T, d>;
  using Num = Num_Comparator<coord>;
  using ID = IDtype;

  PointID() {}
  PointID( const T val ) { this->pnt.fill( val ); }
  PointID( const coords& _pnt ) { this->pnt = _pnt, id = 0; }
  PointID( const coords& _pnt, ID _id ) { this->pnt = _pnt, id = _id; }

  PointID( parlay::slice<T*, T*> x, ID _id ) {
    assert( x.size() == d );
    id = _id;
    for ( int i = 0; i < d; i++ )
      this->pnt[i] = x[i];
  }

  PointID( T* x, ID _id ) {
    id = _id;
    for ( int i = 0; i < d; i++ )
      this->pnt[i] = x[i];
  }

  inline const PointID
  minCoords( const PointID& b ) const {
    coords pts;
    for ( int i = 0; i < d; i++ ) {
      pts[i] = Num::min( this->pnt[i], b.pnt[i] );
    }
    return std::move( PointID( pts ) );
  }

  inline const PointID
  maxCoords( const PointID& b ) const {
    coords pts;
    for ( int i = 0; i < d; i++ ) {
      pts[i] = Num::max( this->pnt[i], b.pnt[i] );
    }
    return std::move( PointID( pts ) );
  }

  inline bool
  operator==( const PointID& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( !Num::Eq( this->pnt[i], x.pnt[i] ) ) return false;
    }
    return this->id == x.id;
  }

  inline bool
  operator<( const PointID& x ) const {
    if ( this->id == x.id ) {
      for ( int i = 0; i < d; i++ ) {
        if ( Num::Lt( this->pnt[i], x.pnt[i] ) )
          return true;
        else if ( Num::Gt( this->pnt[i], x.pnt[i] ) )
          return false;
        else
          continue;
      }
      return false;
    } else {
      return this->id < x.id;
    }
  }

  friend std::ostream&
  operator<<( std::ostream& o, PointID const& a ) {
    o << a.id << "-";
    o << "(";
    for ( int i = 0; i < d; i++ ) {
      o << a.pnt[i] << ", ";
    }
    o << ") " << std::flush;
    return o;
  }

  ID id;
};

}  // namespace cpdd