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
   assert( argc >= 2 );

   int K = 100, LEAVE_WRAP = 16;
   std::string name( argv[1] );
   if( argc >= 3 )
      K = std::stoi( argv[2] );
   if( argc >= 4 )
      LEAVE_WRAP = std::stoi( argv[3] );

   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   parlay::internal::timer timer;

   freopen( argv[1], "r", stdin );
   Point<double>* wp;
   KDtree<double> KD;
   int N, Dim;

   scanf( "%d %d", &N, &Dim );
   assert( N >= K );
   wp = (Point<double>*)malloc( N * sizeof( Point<double> ) );

   for( int i = 0; i < N; i++ )
   {
      for( int j = 0; j < Dim; j++ )
      {
         scanf( "%lf", &wp[i].x[j] );
      }
   }
   timer.start();
   KD.init( Dim, LEAVE_WRAP, wp, N );
   timer.stop();
   std::cout << timer.total_time() << " ";
   timer.reset();

   //* start test
   // std::random_shuffle( wp, wp + N );
   timer.start();
   for( int i = 0; i < N; i++ )
   {
      // printf( "%.6lf\n", std::sqrt( KD.query_k_nearest( &wp[i], K ) ) );
      KD.query_k_nearest( &wp[i], K );
   }
   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << std::endl;

   return 0;
}