#include "common/geometryIO.h"
#include "common/parse_command_line.h"
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

template <typename tree>
void
traverseParallelTree( typename tree::node* root, int deep ) {
   if( root->is_leaf ) {
      aveDeep += deep;
      return;
   }
   typename tree::interior* TI = static_cast<typename tree::interior*>( root );
   traverseParallelTree<tree>( TI->left, deep + 1 );
   traverseParallelTree<tree>( TI->right, deep + 1 );
   return;
}

void
testSerialKDtree( int Dim, int LEAVE_WRAP, Point<Typename>* kdPoint, size_t N,
                  int K ) {
   parlay::internal::timer timer;

   KDtree<Typename> KD;

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
   std::cout << aveDeep / ( N / LEAVE_WRAP ) << " " << aveVisNum << std::endl
             << std::flush;

   //* delete
   // KD.destory( KDroot );
   // delete[] kdPoint;
   // delete[] kdknn;

   return;
}

template <typename tree>
void
testParallelKDtree( int Dim, int LEAVE_WRAP, typename tree::points wp, int N,
                    int K ) {
   using points = typename tree::points;
   using node = typename tree::node;
   parlay::internal::timer timer;
   points wo( wp.size() );
   tree pkd;
   timer.start();
   node* KDParallelRoot =
       pkd.build( wp.cut( 0, wp.size() ), wo.cut( 0, wo.size() ), 0, Dim );
   timer.stop();

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( wp.cut( 0, N ) );
   Typename* kdknn = new Typename[N];

   kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   for( int i = 0; i < N; i++ ) {
      bq[i].resize( K );
   }
   parlay::sequence<double> visNum =
       parlay::sequence<double>::uninitialized( N );
   double aveVisNum = 0.0;

   timer.reset();
   timer.start();

   parlay::parallel_for( 0, N, [&]( size_t i ) {
      size_t visNodeNum = 0;
      pkd.k_nearest( KDParallelRoot, wp[i], Dim, bq[i], visNodeNum );
      kdknn[i] = bq[i].top();
      visNum[i] = ( 1.0 * visNodeNum ) / N;
   } );

   timer.stop();
   std::cout << timer.total_time() << " " << std::flush;

   aveDeep = 0.0;
   traverseParallelTree<tree>( KDParallelRoot, 1 );
   std::cout << aveDeep / ( N / LEAVE_WRAP ) << " "
             << parlay::reduce( visNum.cut( 0, N ) ) << std::endl
             << std::flush;

   // delete_tree( KDParallelRoot );
   // points().swap( wp );
   // points().swap( wo );
   // delete[] bq;
   // delete[] kdknn;

   return;
}

int
main( int argc, char* argv[] ) {
   commandLine P( argc, argv,
                  "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                  "<parallelTag>] [-p <inFile>]" );
   char* iFile = P.getOptionValue( "-p" );
   int K = P.getOptionIntValue( "-k", 100 );
   int Dim = P.getOptionIntValue( "-d", 3 );
   long N = P.getOptionLongValue( "-n", -1 );
   int tag = P.getOptionIntValue( "-t", 1 );

   int LEAVE_WRAP = 16;
   Point<Typename>* wp;

   //* initialize points
   if( iFile != NULL ) {
      std::string name( iFile );
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";

      //* read from file
      parlay::sequence<char> S = readStringFromFile( iFile );
      parlay::sequence<char*> W = stringToWords( S );
      N = atol( W[0] ), Dim = atoi( W[1] );
      assert( N >= 0 && Dim >= 1 && N >= K );

      auto pts = W.cut( 2, W.size() );
      assert( pts.size() % Dim == 0 );
      size_t n = pts.size() / Dim;
      auto a = parlay::tabulate(
          Dim * n, [&]( size_t i ) -> coord { return atol( pts[i] ); } );
      wp = new Point<coord>[N];
      parlay::parallel_for( 0, n, [&]( size_t i ) {
         for( int j = 0; j < Dim; j++ ) {
            wp[i].x[j] = a[i * Dim + j];
         }
      } );
      decltype( a )().swap( a );
   } else { //* construct data byself
      K = 100;
      coord box_size = 10000000;

      std::random_device rd;       // a seed source for the random number engine
      std::mt19937 gen_mt( rd() ); // mersenne_twister_engine seeded with rd()
      std::uniform_int_distribution<int> distrib( 1, box_size );

      parlay::random_generator gen( distrib( gen_mt ) );
      std::uniform_int_distribution<int> dis( 0, box_size );

      wp = new Point<Typename>[N];
      // generate n random points in a cube
      parlay::parallel_for(
          0, N,
          [&]( long i ) {
             auto r = gen[i];
             for( int j = 0; j < Dim; j++ ) {
                wp[i].x[j] = dis( r );
             }
          },
          1000 );
      std::string name =
          std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
      std::cout << name << " ";
   }

   assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

   if( tag == 0 ) //* serial run
      testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
   else if( Dim == 2 ) {
      auto pts = parlay::tabulate(
          N, [&]( size_t i ) -> point2D { return point2D( wp[i].x ); } );
      delete[] wp;
      testParallelKDtree<ParallelKDtree<point2D>>( Dim, LEAVE_WRAP, pts, N, K );
   } else if( Dim == 3 ) {
      auto pts = parlay::tabulate(
          N, [&]( size_t i ) -> point3D { return point3D( wp[i].x ); } );
      delete[] wp;
      testParallelKDtree<ParallelKDtree<point3D>>( Dim, LEAVE_WRAP, pts, N, K );
   } else if( Dim == 5 ) {
      auto pts = parlay::tabulate(
          N, [&]( size_t i ) -> point5D { return point5D( wp[i].x ); } );
      delete[] wp;
      testParallelKDtree<ParallelKDtree<point5D>>( Dim, LEAVE_WRAP, pts, N, K );
   } else if( Dim == 7 ) {
      auto pts = parlay::tabulate(
          N, [&]( size_t i ) -> point7D { return point7D( wp[i].x ); } );
      delete[] wp;
      testParallelKDtree<ParallelKDtree<point7D>>( Dim, LEAVE_WRAP, pts, N, K );
   }

   return 0;
}

template void
testParallelKDtree<ParallelKDtree<point3D>>( int Dim, int LEAVE_WRAP,
                                             ParallelKDtree<point3D>::points wp,
                                             int N, int K );
template void
testParallelKDtree<ParallelKDtree<point5D>>( int Dim, int LEAVE_WRAP,
                                             ParallelKDtree<point5D>::points wp,
                                             int N, int K );
template void
testParallelKDtree<ParallelKDtree<point7D>>( int Dim, int LEAVE_WRAP,
                                             ParallelKDtree<point7D>::points wp,
                                             int N, int K );

template void
traverseParallelTree<ParallelKDtree<point3D>>(
    ParallelKDtree<point3D>::node* root, int deep );

template void
traverseParallelTree<ParallelKDtree<point5D>>(
    ParallelKDtree<point5D>::node* root, int deep );

template void
traverseParallelTree<ParallelKDtree<point7D>>(
    ParallelKDtree<point7D>::node* root, int deep );