#include "kdTree.h"
#include "kdTreeParallel.h"

using Typename = coord;

void
testSerialKDtree( int Dim, int LEAVE_WRAP, points wp, int N, int K ) {
   parlay::internal::timer timer;

   KDtree<Typename> KD;
   Point<Typename>* kdPoint;
   kdPoint = new Point<Typename>[N];
   parlay::parallel_for( 0, N, [&]( size_t i ) {
      for( int j = 0; j < Dim; j++ ) {
         kdPoint[i].x[j] = wp[i].pnt[j];
      }
   } );

   timer.start();
   KDnode<Typename>* KDroot = KD.init( Dim, 16, kdPoint, N );
   timer.stop();

   //? parallel allocate
   // kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   // for( int i = 0; i < N; i++ ) {
   //    bq[i].resize( K );
   // }

   std::cout << timer.total_time() << " ";

   //* start test
   // parlay::random_shuffle( wp.cut( 0, N ) );
   // Typename* kdknn = new Typename[N];

   timer.reset();
   timer.start();
   // parlay::parallel_for( 0, N, [&]( size_t i ) {
   //    KD.k_nearest( KDroot, &wp[i], 0, bq[i] );
   //    kdknn[i] = bq[i].top();
   // } );

   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << " " << K
             << std::endl;

   //* delete
   KD.destory( KDroot );
   points().swap( wp );
   delete[] kdPoint;

   return;
}

void
testParallelKDtree( int Dim, int LEAVE_WRAP, points wp, int N, int K ) {
   parlay::internal::timer timer;
   points wo( wp.size() );

   timer.start();
   node* KDParallelRoot =
       build( wp.cut( 0, wp.size() ), wo.cut( 0, wo.size() ), 0, Dim );
   timer.stop();

   //? parallel allocate
   // kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   // for( int i = 0; i < N; i++ ) {
   //    bq[i].resize( K );
   // }

   std::cout << timer.total_time() << " ";

   //* start test
   // parlay::random_shuffle( wp.cut( 0, N ) );
   // Typename* kdknn = new Typename[N];

   timer.reset();
   timer.start();
   // parlay::parallel_for( 0, N, [&]( size_t i ) {
   //    k_nearest( KDParallelRoot, wp[i], 0, Dim, bq[i] );
   //    kdknn[i] = bq[i].top();
   // } );

   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << " " << K
             << std::endl;

   delete_tree( KDParallelRoot );
   points().swap( wp );
   points().swap( wo );

   return;
}

int
main( int argc, char* argv[] ) {
   assert( argc >= 2 );

   int K = 100, LEAVE_WRAP = 16, Dim;
   long N;
   points wp;
   std::string name( argv[1] );

   //* initialize points
   if( name.find( "/" ) != std::string::npos ) { //* read from file
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";
      auto f = freopen( argv[1], "r", stdin );
      assert( f != nullptr );

      scanf( "%ld %d", &N, &Dim );
      assert( N >= K );
      wp.resize( N );

      for( int i = 0; i < N; i++ ) {
         for( int j = 0; j < Dim; j++ ) {
            scanf( "%ld", &wp[i].pnt[j] );
         }
      }
   } else { //* construct data byself
      parlay::random_generator gen( 0 );
      int box_size = 10000000;
      std::uniform_int_distribution<int> dis( 0, box_size );
      long n = std::stoi( argv[1] );
      N = n;
      Dim = std::stoi( argv[2] );
      try {
         wp.resize( N );
      } catch( ... ) {
         LOG << "catch exception" << ENDL;
         return 0;
      }
      // generate n random points in a cube
      parlay::parallel_for(
          0, n,
          [&]( long i ) {
             auto r = gen[i];
             for( int j = 0; j < Dim; j++ ) {
                wp[i].pnt[j] = dis( r );
             }
          },
          1000 );
      name = std::to_string( n ) + "_" + std::to_string( Dim ) + ".in";
      std::cout << name << " ";
   }

   // if( argc >= 4 )
   //    K = std::stoi( argv[3] );
   // if( argc >= 5 )
   //    LEAVE_WRAP = std::stoi( argv[4] );
   assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

   if( argc >= 4 ) {
      if( std::stoi( argv[3] ) == 1 )
         testParallelKDtree( Dim, LEAVE_WRAP, wp, N, K );
      else if( std::stoi( argv[3] ) == 0 )
         testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
   }

   return 0;
}