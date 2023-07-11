#include "kdTree.h"
#include "kdTreeParallel.h"

using Typename = coord;

double aveDeep = 0.0;

void
traverseSerialTree( KDnode<Typename>* KDroot, int deep ) {
   if( KDroot->isLeaf ) {
      aveDeep += deep;
      return;
   }
   traverseSerialTree( KDroot->left, deep + 1 );
   traverseSerialTree( KDroot->right, deep + 1 );
   return;
}

void
testSerialKDtree( int Dim, int LEAVE_WRAP, points wp, size_t N, int K ) {
   parlay::internal::timer timer;

   KDtree<Typename> KD;
   Point<Typename>* kdPoint;
   kdPoint = new Point<Typename>[N];
   parlay::parallel_for( 0, N, [&]( size_t i ) {
      for( int j = 0; j < Dim; j++ ) {
         kdPoint[i].x[j] = wp[i].pnt[j];
      }
   } );
   points().swap( wp );

   timer.start();
   KDnode<Typename>* KDroot = KD.init( Dim, LEAVE_WRAP, kdPoint, N );
   timer.stop();

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( parlay::make_slice( kdPoint, kdPoint + N ) );
   Typename* kdknn = new Typename[N];

   timer.reset();
   timer.start();
   kBoundedQueue<Typename> bq;
   double aveVisNum = 0.0;
   for( size_t i = 0; i < N; i++ ) {
      size_t visNodeNum = 0;
      bq.resize( K );
      KD.k_nearest( KDroot, &kdPoint[i], 0, bq, visNodeNum );
      kdknn[i] = bq.top();
      aveVisNum += ( 1.0 * visNodeNum ) / N;
   }

   timer.stop();
   std::cout << timer.total_time() << " " << std::flush;

   aveDeep = 0.0;
   traverseSerialTree( KDroot, 1 );
   std::cout << aveDeep / ( N / LEAVE_WRAP ) << " " << aveVisNum << std::endl;

   //* delete
   // KD.destory( KDroot );
   // delete[] kdPoint;
   // delete[] kdknn;

   return;
}

void
testParallelKDtree( int Dim, int LEAVE_WRAP, points wp, int N, int K ) {
   parlay::internal::timer timer;
   points wo( wp.size() );

   timer.start();
   std::array<coord, PIVOT_NUM> pivots;
   std::array<int, PIVOT_NUM> sums;

   node* KDParallelRoot = build( wp.cut( 0, wp.size() ), wo.cut( 0, wo.size() ),
                                 0, Dim, pivots, 0, sums );
   timer.stop();

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( wp.cut( 0, N ) );
   Typename* kdknn = new Typename[N];

   kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   for( int i = 0; i < N; i++ ) {
      bq[i].resize( K );
   }
   timer.reset();
   timer.start();

   parlay::parallel_for( 0, N, [&]( size_t i ) {
      size_t visNodeNum = 0;
      k_nearest( KDParallelRoot, wp[i], Dim, bq[i], visNodeNum );
      kdknn[i] = bq[i].top();
   } );

   timer.stop();
   std::cout << timer.total_time() << " " << LEAVE_WRAP << " " << K
             << std::endl;

   // delete_tree( KDParallelRoot );
   // points().swap( wp );
   // points().swap( wo );
   // delete[] bq;
   // delete[] kdknn;

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
      K = std::stoi( argv[2] );
      assert( f != nullptr );

      scanf( "%ld%d", &N, &Dim );
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

   assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

   if( argc >= 4 ) {
      if( std::stoi( argv[3] ) == 1 )
         testParallelKDtree( Dim, LEAVE_WRAP, wp, N, K );
      else if( std::stoi( argv[3] ) == 0 )
         testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
   }

   return 0;
}