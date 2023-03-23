#include <bits/stdc++.h>

const double eps = 1e-9;

inline bool
Gt( const double& a, const double& b )
{
   return a - b > eps;
}

inline bool
Lt( const double& a, const double& b )
{
   return a - b < -eps;
}

inline bool
Eq( const double& a, const double& b )
{
   return std::abs( a - b ) < eps;
}

inline bool
Geq( const double& a, const double& b )
{
   return Gt( a, b ) || Eq( a, b );
}

inline bool
Leq( const double& a, const double& b )
{
   return Lt( a, b ) || Eq( a, b );
}

class kArrayQueue
{
 public:
   void
   init( const int& k )
   {
      free( Q );
      Q = (double*)malloc( ( 2 * k ) * sizeof( double ) );
      for( int i = 0; i < 2 * k; i++ )
      {
         Q[i] = DBL_MAX;
      }
      K = k;
      load = 0;
      size = 2 * k;
      return;
   }

   void
   insert( double d )
   {
      Q[load++] = d;
      if( load == size )
      {
         std::nth_element( Q, Q + ( size / 2 - 1 ), Q + size );
         load /= 2;
      }
      return;
   }

   double
   queryKthElement()
   {
      std::nth_element( Q, Q + ( size / 2 - 1 ), Q + size );
      return Q[size / 2 - 1];
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
   double* Q;
   int K;
   int load;
   int size;
};