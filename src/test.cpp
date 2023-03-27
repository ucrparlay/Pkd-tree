#include <parlay/delayed_sequence.h>
#include <parlay/internal/get_time.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

#include <iostream>

#include "kdTree.h"

int
main( int argc, char* argv[] )
{

   parlay::internal::timer timer;
   assert( argc >= 2 );

   std::string name( argv[1] );
   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   freopen( argv[1], "r", stdin );
   Point<double>* wp;
   KDtree<double> KD;
   int N, Q, K, Dim, LEAVE_WRAP = 16;
   if( argc >= 3 )
      LEAVE_WRAP = std::stoi( argv[2] );

   scanf( "%d %d", &N, &Dim );
   wp = (Point<double>*)malloc( N * sizeof( Point<double> ) );

   for( int i = 0; i < N; i++ )
   {
      for( int j = 0; j < Dim; j++ )
      {
         scanf( "%lf", &wp[i].x[j] );
      }
   }

   KD.init( Dim, LEAVE_WRAP, wp, N );

   int i, j;
   scanf( "%d", &Q );
   Point<double> z;
   while( Q-- )
   {
      scanf( "%d", &K );
      for( j = 0; j < Dim; j++ )
         scanf( "%lf", &z.x[j] );

      // puts( ">>>>" );

      double ans = KD.query_k_nearest( &z, K );
      printf( "%.6lf\n", sqrt( ans ) );

      // k_nearest_array( root, &z, 0, Dim );

      // printf( "%.6lf\n", sqrt( kq.queryKthElement() ) );
      // printf( "%.6lf\n", sqrt( q.top() ) );
   }
   return 0;
}