#pragma once

#include <bits/stdc++.h>
#include <unistd.h>

#include "parlay/alloc.h"
#include "parlay/delayed.h"
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/utilities.h"

//*---------- point definition ------------------
using coord = double;  // type of each coordinate
constexpr double eps = 1e-7;

//! type with T could be really slow
template<typename T, uint_fast8_t d>
struct PointType {
  using coord = T;
  using coords = std::array<T, d>;

  PointType() {}

  PointType( const T& val ) { pnt.fill( val ); }

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

  inline bool
  Lt( const T& a, const T& b ) const {
    if constexpr ( std::is_integral_v<T> )
      return a < b;
    else if ( std::is_floating_point_v<T> )
      return a - b < -eps;
  }

  inline bool
  Eq( const T& a, const T& b ) const {
    if constexpr ( std::is_integral_v<T> )
      return a == b;
    else if ( std::is_floating_point_v<T> )
      return std::abs( a - b ) < eps;
  }

  inline bool
  Gt( const T& a, const T& b ) const {
    if constexpr ( std::is_integral_v<T> )
      return a > b;
    else if ( std::is_floating_point_v<T> ) {
      return a - b > eps;
    }
  }

  bool
  operator==( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( !Eq( pnt[i], x.pnt[i] ) ) return false;
    }
    return true;
  }

  bool
  operator<( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      // if ( pnt[i] < x.pnt[i] )
      if ( Lt( pnt[i], x.pnt[i] ) ) return true;
      // else if ( pnt[i] > x.pnt[i] )
      else if ( Gt( pnt[i], x.pnt[i] ) )
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

//*----------- double precision comparision ----------------

template<typename T>
class Comparator {
 public:
  static inline bool
  Gt( const T& a, const T& b ) {
    return a - b > eps;
  }

  static inline bool
  Lt( const T& a, const T& b ) {
    return a - b < -eps;
  }

  static inline bool
  Eq( const T& a, const T& b ) {
    return std::abs( a - b ) < eps;
  }

  static inline bool
  Geq( const T& a, const T& b ) {
    return Gt( a, b ) || Eq( a, b );
  }

  static inline bool
  Leq( const T& a, const T& b ) {
    return Lt( a, b ) || Eq( a, b );
  }

 private:
  static constexpr double eps = 1e-7;
};

template<typename T>
class kArrayQueue {
 public:
  //* build a k-nn graph for N points
  void
  resize( const int& k ) {
    Q = (T*)malloc( 2 * k * sizeof( T ) );
    for ( int i = 0; i < 2 * k; i++ ) {
      Q[i] = std::numeric_limits<T>::max();
    }
    K = k;
    load = 0;
    size = 2 * k;
    return;
  };

  void
  init( const int& N ) {
    Q = (T*)malloc( N * sizeof( T ) );
  }

  void
  set( const int& k ) {
    for ( int i = 0; i < 2 * k; i++ ) {
      Q[i] = std::numeric_limits<T>::max();
    }
    K = k;
    load = 0;
    size = 2 * k;
    return;
  }

  void
  insert( T d ) {
    Q[load++] = d;
    if ( load == size ) {
      std::nth_element( Q, Q + K - 1, Q + size );
      load = load / 2;
    }
    return;
  }

  T
  queryKthElement() {
    if ( load < K ) {
      return Q[load];
    } else {
      std::nth_element( Q, Q + K - 1, Q + size );
      return Q[K - 1];
    }
  }

  void
  destory() {
    free( Q );
  }

  inline const int&
  getLoad() {
    return load;
  }

  void
  printQueue() {
    for ( int i = 0; i <= K; i++ ) {
      if ( Q[i] == DBL_MAX )
        printf( "inf " );
      else
        printf( "%.2f ", Q[i] );
    }
    puts( "" );
  }

 public:
  T* Q;
  int* L;
  int K;
  int load;
  int size;
};

template<typename T>
class ArrayQueue {
 public:
  void
  init( const int& N ) {
    Q = (T*)malloc( N * sizeof( T ) );
  }

  void
  set( const int& k ) {
    for ( int i = 0; i < k; i++ ) {
      Q[i] = std::numeric_limits<T>::max();
    }
    K = k;
    load = 0;
    return;
  }

  void
  insert( T d ) {
    //* highest to lowest by distance d
    if ( Lt( d, Q[0] ) ) {
      Q[0] = d;
      load++;
      load = std::min( load, K );
      for ( int i = 1; i < K && Q[i] > Q[i - 1]; i++ ) {
        std::swap( Q[i], Q[i - 1] );
      }
    }
    return;
  }

  inline const T&
  top() {
    return Q[0];
  }

  void
  destory() {
    free( Q );
  }

  inline bool
  full() {
    return load == K;
  }

 public:
  T* Q;
  int K;
  int load;
};

template<typename T, typename Compare = std::less<T>>
class kBoundedQueue {
  /*
   * A priority queue with fixed maximum capacity.
   * While the queue has not reached its maximum capacity, elements are
   * inserted as they will be in a heap, the root (top()) being such that
   * Compare(top(),x)=false for any x in the queue.
   * Once the queue is full, trying to insert x in the queue will have no
   * effect if Compare(x,top())=false. Otherwise, the element at the root of
   * the heap is removed and x is inserted so as to keep the heap property.
   */
  //* A simplified version of CGAL bounded_priority_queue
  //* https://github.com/CGAL/cgal/blob/v5.4/Spatial_searching/include/CGAL/Spatial_searching/internal/bounded_priority_queue.h

 public:
  kBoundedQueue( const Compare& comp = Compare() ) : m_comp( comp ) {}

  kBoundedQueue( parlay::slice<T*, T*> data_slice, const Compare& comp = Compare() ) :
      m_count( 0 ), m_data( data_slice ), m_comp( comp ) {}

  /** Sets the max number of elements in the queue */
  void
  resize( parlay::slice<T*, T*> data_slice ) {
    m_data = data_slice;
    m_count = 0;
  }

  void
  reset() {
    m_count = 0;
  }

  inline bool
  full() const {
    return m_count == m_data.size();
  }

  inline const T&
  top() const {
    return m_data[0];
  }

  static Comparator<T> Num;

  inline void
  insert( const T x ) {
    T* data1 = ( &m_data[0] - 1 );
    if ( full() ) {
      if ( Num.Lt( x, top() ) ) {
        // insert x in the heap at the correct place,
        // going down in the tree.
        size_t j( 1 ), k( 2 );
        while ( k <= m_count ) {
          T* z = &( data1[k] );
          if ( ( k < m_count ) && Num.Lt( *z, data1[k + 1] ) ) z = &( data1[++k] );

          if ( Num.Lt( *z, x ) ) break;
          data1[j] = *z;
          j = k;
          k = j << 1;  // a son of j in the tree
        }
        data1[j] = x;
      }
    } else {
      // insert element as in a heap
      size_t i( ++m_count ), j;
      while ( i >= 2 ) {
        j = i >> 1;  // father of i in the tree
        T& y = data1[j];
        if ( Num.Lt( x, y ) ) break;
        data1[i] = y;
        i = j;
      }
      data1[i] = x;
    }
  }

 public:
  size_t m_count = 0;
  parlay::slice<T*, T*> m_data;
  // parlay::sequence<T> m_data;
  Compare m_comp;
};