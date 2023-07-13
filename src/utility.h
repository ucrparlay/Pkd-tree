#pragma once

#include <bits/stdc++.h>

#include "parlay/alloc.h"
#include "parlay/delayed.h"
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/utilities.h"

constexpr double eps = 1e-7;

template <typename T>
inline bool
Gt( const T& a, const T& b ) {
   if constexpr( std::is_integral_v<T> )
      return a > b;
   else if( std::is_floating_point_v<T> ) {
      return a - b > eps;
   }
}

template <typename T>
inline bool
Lt( const T& a, const T& b ) {
   if constexpr( std::is_integral_v<T> )
      return a < b;
   else if( std::is_floating_point_v<T> )
      return a - b < -eps;
}

template <typename T>
inline bool
Eq( const T& a, const T& b ) {
   if constexpr( std::is_integral_v<T> )
      return a == b;
   else if( std::is_floating_point_v<T> )
      return std::abs( a - b ) < eps;
}

template <typename T>
inline bool
Geq( const T& a, const T& b ) {
   if constexpr( std::is_integral_v<T> )
      return a >= b;
   else if( std::is_floating_point_v<T> )
      return Gt( a, b ) || Eq( a, b );
}

template <typename T>
inline bool
Leq( const T& a, const T& b ) {
   if constexpr( std::is_integral_v<T> )
      return a <= b;
   else if( std::is_floating_point_v<T> )
      return Lt( a, b ) || Eq( a, b );
}

template <typename T>
class kArrayQueue {
 public:
   //* build a k-nn graph for N points
   void
   resize( const int& k ) {
      Q = (T*)malloc( 2 * k * sizeof( T ) );
      for( int i = 0; i < 2 * k; i++ ) {
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
      for( int i = 0; i < 2 * k; i++ ) {
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
      if( load == size ) {
         std::nth_element( Q, Q + K - 1, Q + size );
         load = load / 2;
      }
      return;
   }

   T
   queryKthElement() {
      if( load < K ) {
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
      for( int i = 0; i <= K; i++ ) {
         if( Q[i] == DBL_MAX )
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

template <typename T>
class ArrayQueue {
 public:
   void
   init( const int& N ) {
      Q = (T*)malloc( N * sizeof( T ) );
   }

   void
   set( const int& k ) {
      for( int i = 0; i < k; i++ ) {
         Q[i] = std::numeric_limits<T>::max();
      }
      K = k;
      load = 0;
      return;
   }

   void
   insert( T d ) {
      //* highest to lowest by distance d
      if( Lt( d, Q[0] ) ) {
         Q[0] = d;
         load++;
         load = std::min( load, K );
         for( int i = 1; i < K && Q[i] > Q[i - 1]; i++ ) {
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

template <typename T, typename Compare = std::less<T>>
class kBoundedQueue {
   //* A simplified version of CGAL bounded_priority_queue
   //* https://github.com/CGAL/cgal/blob/v5.4/Spatial_searching/include/CGAL/Spatial_searching/internal/bounded_priority_queue.h

 public:
   kBoundedQueue( const Compare& comp = Compare() ) : m_comp( comp ) {}

   kBoundedQueue( int size, const Compare& comp = Compare() )
       : m_count( 0 ), m_data( size ), m_comp( comp ) {
      // a.allocate( size );
   }

   /** Sets the max number of elements in the queue */
   void
   resize( int new_size ) {
      m_data.clear();
      if( m_data.size() != new_size )
         m_data.resize( new_size );
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
      if( full() ) {
         if( m_comp( x, top() ) ) {
            // insert x in the heap at the correct place,
            // going down in the tree.
            unsigned int j( 1 ), k( 2 );
            while( k <= m_count ) {
               T* z = &( data1[k] );
               if( ( k < m_count ) && m_comp( *z, data1[k + 1] ) )
                  z = &( data1[++k] );

               if( m_comp( *z, x ) )
                  break;
               data1[j] = *z;
               j = k;
               k = j << 1; // a son of j in the tree
            }
            data1[j] = x;
         }
      } else {
         // insert element as in a heap
         int i( ++m_count ), j;
         while( i >= 2 ) {
            j = i >> 1; // father of i in the tree
            T& y = data1[j];
            if( m_comp( x, y ) )
               break;
            data1[i] = y;
            i = j;
         }
         data1[i] = x;
      }
   }

 public:
   unsigned int m_count;
   parlay::sequence<T> m_data;
   Compare m_comp;
};