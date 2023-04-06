#include <parlay/delayed_sequence.h>
#include <parlay/internal/get_time.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

#include <iostream>

#include "kdTree.h"
#include "kdTreeParallel.h"

using Typename = long;

void
testSequential( int Dim, int LEAVE_WRAP, Point<Typename>* wp, int N, int K )
{
   parlay::internal::timer timer;
   KDtree<Typename> KD;
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
   assert( argc >= 2 );

   int K = 100, LEAVE_WRAP = 16, N, Dim;
   Point<Typename>* wp;
   if( argc >= 4 )
      K = std::stoi( argv[3] );
   if( argc >= 5 )
      LEAVE_WRAP = std::stoi( argv[4] );

   std::string name( argv[1] );
   if( name.find( "/" ) != std::string::npos )
   { //* read from file
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";
      auto f = freopen( argv[1], "r", stdin );
      assert( f != nullptr );

      scanf( "%d %d", &N, &Dim );
      assert( N >= K );
      wp = (Point<Typename>*)malloc( N * sizeof( Point<Typename> ) );

      for( int i = 0; i < N; i++ )
      {
         wp[i].id = i;
         for( int j = 0; j < Dim; j++ )
         {
            //! change read type if int
            scanf( "%lf", &wp[i].x[j] );
         }
      }
   }
   else
   { //* construct data byself
      parlay::random_generator gen( 0 );
      int box_size = 1000000000;
      std::uniform_int_distribution<int> dis( 0, box_size );
      long n = std::stoi( argv[1] );
      Dim = std::stoi( argv[2] );
      wp = new Point<Typename>[n];
      // generate n random points in a cube
      parlay::parallel_for( 0, n,
                            [&]( long i )
                            {
                               auto r = gen[i];
                               for( int j = 0; j < Dim; j++ )
                               {
                                  wp[i].x[j] = dis( r );
                               }
                            } );
      // for( int i = 0; i < n; i++ )
      // {
      //    wp[i].print();
      //    puts( "" );
      // }
   }

   testSequential( Dim, LEAVE_WRAP, wp, N, K );
   return 0;
}