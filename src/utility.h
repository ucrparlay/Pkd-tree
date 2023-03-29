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
      Q[0] = -1;
      for( int i = 1; i <= 2 * k; i++ )
      {
         Q[i] = std::numeric_limits<T>::max();
      }
      K = k;
      load = 0;
      size = 2 * k;
      return;
   }
   inline void
   insert( T d )
   {
      Q[++load] = d;
      if( load == size )
      {
         std::nth_element( Q + 1, Q + K, Q + size + 1 );
         load = load / 2;
      }
      return;
   }
   inline T
   queryKthElement()
   {
      //! wrong here, check the when load size < k
      std::nth_element( Q + 1, Q + K, Q + size + 1 );
      return Q[K];
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
      for( int i = 0; i <= K; i++ )
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