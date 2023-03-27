#include <bits/stdc++.h>

const double eps = 1e-7;

template <typename T>
inline bool
Gt( const T& a, const T& b )
{
   return a - b > eps;
}

template <typename T>
inline bool
Lt( const T& a, const T& b )
{
   return a - b < -eps;
}

template <typename T>
inline bool
Eq( const T& a, const T& b )
{
   return std::abs( a - b ) < eps;
}

template <typename T>
inline bool
Geq( const T& a, const T& b )
{
   return Gt( a, b ) || Eq( a, b );
}

template <typename T>
inline bool
Leq( const T& a, const T& b )
{
   return Lt( a, b ) || Eq( a, b );
}

template <typename T>
class kArrayQueue
{
 public:
   void
   init( const int& N )
   {
      Q = (T*)malloc( N * sizeof( T ) );
   }
   void
   set( const int& k )
   {
      for( int i = 0; i < 2 * k; i++ )
      {
         Q[i] = DBL_MAX;
      }
      K = k;
      load = 0;
      size = 2 * k;
      return;
   }
   inline void
   insert( T d )
   {
      Q[load++] = d;
      if( load == size )
      {
         std::nth_element( Q, Q + ( size / 2 - 1 ), Q + size );
         load /= 2;
      }
      return;
   }
   inline T
   queryKthElement()
   {
      std::nth_element( Q, Q + ( size / 2 - 1 ), Q + size );
      return Q[size / 2 - 1];
   }

   void
   destory()
   {
      free( Q );
   }

   inline const int&
   getLoad()
   {
      return load;
   }

   void
   printQueue()
   {
      for( int i = 0; i < size; i++ )
      {
         if( Q[i] == DBL_MAX )
            printf( "inf " );
         else
            printf( "%.2f ", Q[i] );
      }
      puts( "" );
   }

 public:
   T* Q;
   int K;
   int load;
   int size;
};