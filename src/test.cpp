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
testParallelkArray( int Dim, int LEAVE_WRAP,
                    parlay::sequence<Point<Typename>> wp, int N, int K )
{
   parlay::internal::timer timer;
   KDtree<Typename> KD;
   timer.start();
   KDnode<Typename>* KDroot = KD.init( Dim, LEAVE_WRAP, wp, N );
   kArrayQueue<Typename>* kq = new kArrayQueue<Typename>[N];
   for( int i = 0; i < N; i++ )
   {
      kq[i].resize( K );
   }
   timer.stop();
   std::cout << timer.total_time() << " ";
   timer.reset();
   //* start test
   std::random_shuffle( wp.begin(), wp.begin() + N );

   timer.reset();
   timer.start();
   parlay::parallel_for( 0, N,
                         [&]( size_t i )
                         { KD.k_nearest_array( KDroot, &wp[i], 0, kq[i] ); } );

   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << " " << K
             << std::endl;

   KD.destory( KDroot );
   delete[] kq;
   wp = parlay::sequence<Point<Typename>>();
}

void
testParallel( int Dim, int LEAVE_WRAP, parlay::sequence<Point<Typename>> wp,
              int N, int K )
{
   parlay::internal::timer timer;
   KDtree<Typename> KD;
   timer.start();
   KDnode<Typename>* KDroot = KD.init( Dim, LEAVE_WRAP, wp, N );
   kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   for( int i = 0; i < N; i++ )
   {
      bq[i].resize( K );
   }
   timer.stop();

   std::cout << timer.total_time() << " ";

   //* start test
   std::random_shuffle( wp.begin(), wp.begin() + N );

   timer.reset();
   timer.start();
   parlay::parallel_for(
       0, N, [&]( size_t i ) { KD.k_nearest( KDroot, &wp[i], 0, bq[i] ); } );

   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << " " << K
             << std::endl;

   KD.destory( KDroot );
   delete[] bq;
   wp = parlay::sequence<Point<Typename>>();
}

int
main( int argc, char* argv[] )
{
   parlay::sequence<int> p{ 5, 1, 4, 3, 2 };
   parlay::sequence<bool> flag{ 0, 1, 0, 1, 1 };
   auto pp = p.cut( 0, 2 );
   for( auto i : pp )
   {
      LOG << i << ENDL;
   }
   return 0;

   assert( argc >= 2 );

   int K = 100, LEAVE_WRAP = 16, N, Dim;
   parlay::sequence<Point<Typename>> wp;
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
      wp.resize( N );
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
      N = n;
      Dim = std::stoi( argv[2] );
      wp.resize( N );
      // generate n random points in a cube
      parlay::parallel_for(
          0, n,
          [&]( long i )
          {
             auto r = gen[i];
             for( int j = 0; j < Dim; j++ )
             {
                wp[i].x[j] = dis( r );
             }
          },
          1000 );
      // for( int i = 0; i < n; i++ )
      // {
      //    wp[i].print();
      //    puts( "" );
      // }
      name = std::to_string( n ) + "_" + std::to_string( Dim ) + ".in";
      std::cout << name << " ";
   }

   if( argc >= 6 && std::stoi( argv[5] ) == 0 )
   {
      testParallel( Dim, LEAVE_WRAP, wp, N, K );
   }
   if( argc >= 6 && std::stoi( argv[5] ) == 1 )
   {
      testParallelkArray( Dim, LEAVE_WRAP, wp, N, K );
   }
   return 0;
}