#include <parlay/delayed_sequence.h>
#include <parlay/internal/get_time.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

#include <iostream>

#include "kdTree.h"
#include "kdTreeParallel.h"

void
testSequential( int argc, char* argv[] )
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

   auto f = freopen( argv[1], "r", stdin );
   assert( f != nullptr );
   Point<double>* wp;
   KDtree<double> KD;
   int N, Dim;

   scanf( "%d %d", &N, &Dim );
   assert( N >= K );
   wp = (Point<double>*)malloc( N * sizeof( Point<double> ) );

   for( int i = 0; i < N; i++ )
   {
      wp[i].id = i;
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
   std::random_shuffle( wp, wp + N );
   timer.start();
   for( int i = 0; i < N; i++ )
   {
      // KD.query_k_nearest_array( &wp[i], K );
      KD.query_k_nearest( &wp[i], K );
   }
   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << std::endl;
}

void
testParallel( int argc, char* argv[] )
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

   auto f = freopen( argv[1], "r", stdin );
   assert( f != nullptr );
   Point<double>* wp;
   KDtree<double> KD;
   int N, Dim;

   scanf( "%d %d", &N, &Dim );
   assert( N >= K );
   wp = (Point<double>*)malloc( N * sizeof( Point<double> ) );

   for( int i = 0; i < N; i++ )
   {
      wp[i].id = i;
      for( int j = 0; j < Dim; j++ )
      {
         scanf( "%lf", &wp[i].x[j] );
      }
   }
   KDnode<double>* KDroot;
   timer.start();
   KDroot = KD.init( Dim, LEAVE_WRAP, wp, N );
   timer.stop();
   std::cout << timer.total_time() << " ";
   timer.reset();

   //* start test
   std::random_shuffle( wp, wp + N );
   timer.start();
   parlay::sequence<kArrayQueue<double>> kq( N );
   for( int i = 0; i < N; i++ )
   {
      kq[i].resize( K );
   }
   parlay::parallel_for( 0, N,
                         [&]( size_t i )
                         { KD.k_nearest_array( KDroot, &wp[i], 0, kq[i] ); } );

   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << std::endl;
}

int
main( int argc, char* argv[] )
{
   // parlay::sequence<int> p = { 5, 10, 6, 4, 3, 2, 6, 7, 9, 3 };
   // auto s = p.cut( 0, p.size() );
   // auto m = s.begin() + s.size() / 2;
   // std::nth_element( s.begin(), m, s.end() );
   // for( int i = 0; i < 10; i++ )
   // {
   //    std::cout << p[i] << std::endl;
   // }
   testSequential( argc, argv );
   return 0;
}