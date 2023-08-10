#pragma once

#include <bits/stdc++.h>
#include <unistd.h>

#include "parlay/alloc.h"
#include "parlay/delayed.h"
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/utilities.h"

//*---------- point definition ------------------
using coord = long;  // type of each coordinate

//! type with T could be really slow
template<typename T, int d>
struct PointType {
  using coords = std::array<T, d>;
  PointType() {}

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

  bool
  operator==( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( pnt[i] != x.pnt[i] ) return false;
    }
    return true;
  }

  bool
  operator<( const PointType& x ) const {
    for ( int i = 0; i < d; i++ ) {
      if ( pnt[i] < x.pnt[i] )
        return true;
      else if ( pnt[i] > x.pnt[i] )
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

struct point2D {
  using coords = std::array<coord, 2>;
  point2D() {}
  point2D( const coords& _pnt ) : pnt( _pnt ){};
  point2D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point2D( coord* x ) {
    for ( int i = 0; i < 2; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};
struct point3D {
  using coords = std::array<coord, 3>;
  point3D() {}
  point3D( const coords& _pnt ) : pnt( _pnt ){};
  point3D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point3D( coord* x ) {
    for ( int i = 0; i < 3; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};
struct point5D {
  using coords = std::array<coord, 5>;
  point5D() {}
  point5D( const coords& _pnt ) : pnt( _pnt ){};
  point5D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point5D( coord* x ) {
    for ( int i = 0; i < 5; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};
struct point7D {
  using coords = std::array<coord, 7>;
  point7D() {}
  point7D( const coords& _pnt ) : pnt( _pnt ){};
  point7D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point7D( coord* x ) {
    for ( int i = 0; i < 7; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};
struct point9D {
  using coords = std::array<coord, 9>;
  point9D() {}
  point9D( const coords& _pnt ) : pnt( _pnt ){};
  point9D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point9D( coord* x ) {
    for ( int i = 0; i < 9; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};
struct point10D {
  using coords = std::array<coord, 10>;
  point10D() {}
  point10D( const coords& _pnt ) : pnt( _pnt ){};
  point10D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point10D( coord* x ) {
    for ( int i = 0; i < 10; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};
struct point15D {
  using coords = std::array<coord, 15>;
  point15D() {}
  point15D( const coords& _pnt ) : pnt( _pnt ){};
  point15D( parlay::slice<coord*, coord*> x ) {
    for ( int i = 0; i < x.size(); i++ )
      pnt[i] = x[i];
  }
  point15D( coord* x ) {
    for ( int i = 0; i < 15; i++ )
      pnt[i] = x[i];
  }
  coords pnt;
};

//*----------- double precision comparision ----------------
constexpr double eps = 1e-7;

template<typename T>
inline bool
Gt( const T& a, const T& b ) {
  if constexpr ( std::is_integral_v<T> )
    return a > b;
  else if ( std::is_floating_point_v<T> ) {
    return a - b > eps;
  }
}

template<typename T>
inline bool
Lt( const T& a, const T& b ) {
  if constexpr ( std::is_integral_v<T> )
    return a < b;
  else if ( std::is_floating_point_v<T> )
    return a - b < -eps;
}

template<typename T>
inline bool
Eq( const T& a, const T& b ) {
  if constexpr ( std::is_integral_v<T> )
    return a == b;
  else if ( std::is_floating_point_v<T> )
    return std::abs( a - b ) < eps;
}

template<typename T>
inline bool
Geq( const T& a, const T& b ) {
  if constexpr ( std::is_integral_v<T> )
    return a >= b;
  else if ( std::is_floating_point_v<T> )
    return Gt( a, b ) || Eq( a, b );
}

template<typename T>
inline bool
Leq( const T& a, const T& b ) {
  if constexpr ( std::is_integral_v<T> )
    return a <= b;
  else if ( std::is_floating_point_v<T> )
    return Lt( a, b ) || Eq( a, b );
}

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

  inline void
  insert( const T x ) {
    T* data1 = ( &m_data[0] - 1 );
    if ( full() ) {
      if ( m_comp( x, top() ) ) {
        // insert x in the heap at the correct place,
        // going down in the tree.
        size_t j( 1 ), k( 2 );
        while ( k <= m_count ) {
          T* z = &( data1[k] );
          if ( ( k < m_count ) && m_comp( *z, data1[k + 1] ) ) z = &( data1[++k] );

          if ( m_comp( *z, x ) ) break;
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
        if ( m_comp( x, y ) ) break;
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