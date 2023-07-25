#include "testFramework.h"

void testSerialKDtree( int Dim, int LEAVE_WRAP, Point<Typename>* kdPoint, size_t N,
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
  for ( size_t i = 0; i < N; i++ ) {
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

template<typename tree>
void testParallelKDtree( const int& Dim, const int& LEAVE_WRAP,
                         const typename tree::points& wp, const int& N, const int& K,
                         const int& rounds ) {
  using points = typename tree::points;
  using node = typename tree::node;
  assert( N == wp.size() );
  tree pkd;

  buildTree( Dim, wp, rounds, pkd );
  return;

  Typename* kdknn = new Typename[N];
  queryKNN( Dim, wp, rounds, pkd, kdknn, K );
  std::cout << std::endl;

  return;
}

int main( int argc, char* argv[] ) {
  commandLine P( argc, argv,
                 "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                 "<parallelTag>] [-p <inFile>] [-r {1,...,5}]" );
  char* iFile = P.getOptionValue( "-p" );
  int K = P.getOptionIntValue( "-k", 100 );
  int Dim = P.getOptionIntValue( "-d", 3 );
  long N = P.getOptionLongValue( "-n", -1 );
  int tag = P.getOptionIntValue( "-t", 1 );
  int rounds = P.getOptionIntValue( "-r", 3 );

  int LEAVE_WRAP = 32;
  Point<Typename>* wp;

  //* initialize points
  if ( iFile != NULL ) {
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
    auto a =
        parlay::tabulate( Dim * n, [&]( size_t i ) -> coord { return atol( pts[i] ); } );
    wp = new Point<coord>[N];
    parlay::parallel_for( 0, n, [&]( size_t i ) {
      for ( int j = 0; j < Dim; j++ ) {
        wp[i].x[j] = a[i * Dim + j];
      }
    } );
    decltype( a )().swap( a );
  } else {  //* construct data byself
    K = 100;
    coord box_size = 10000000;

    std::random_device rd;        // a seed source for the random number engine
    std::mt19937 gen_mt( rd() );  // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int> distrib( 1, box_size );

    parlay::random_generator gen( distrib( gen_mt ) );
    std::uniform_int_distribution<int> dis( 0, box_size );

    wp = new Point<Typename>[N];
    // generate n random points in a cube
    parlay::parallel_for(
        0, N,
        [&]( long i ) {
          auto r = gen[i];
          for ( int j = 0; j < Dim; j++ ) {
            wp[i].x[j] = dis( r );
          }
        },
        1000 );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

  if ( tag == 0 )  //* serial run
    testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
  else if ( Dim == 2 ) {
    auto pts =
        parlay::tabulate( N, [&]( size_t i ) -> point2D { return point2D( wp[i].x ); } );
    delete[] wp;
    testParallelKDtree<ParallelKDtree<point2D>>( Dim, LEAVE_WRAP, pts, N, K, rounds );
  } else if ( Dim == 3 ) {
    auto pts =
        parlay::tabulate( N, [&]( size_t i ) -> point3D { return point3D( wp[i].x ); } );
    delete[] wp;
    testParallelKDtree<ParallelKDtree<point3D>>( Dim, LEAVE_WRAP, pts, N, K, rounds );
  } else if ( Dim == 5 ) {
    auto pts =
        parlay::tabulate( N, [&]( size_t i ) -> point5D { return point5D( wp[i].x ); } );
    delete[] wp;
    testParallelKDtree<ParallelKDtree<point5D>>( Dim, LEAVE_WRAP, pts, N, K, rounds );
  } else if ( Dim == 7 ) {
    auto pts =
        parlay::tabulate( N, [&]( size_t i ) -> point7D { return point7D( wp[i].x ); } );
    delete[] wp;
    testParallelKDtree<ParallelKDtree<point7D>>( Dim, LEAVE_WRAP, pts, N, K, rounds );
  }

  return 0;
}
