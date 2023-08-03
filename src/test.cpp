#include "testFramework.h"

void
testSerialKDtree( int Dim, int LEAVE_WRAP, Point<Typename>* kdPoint, size_t N, int K ) {
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
  double aveVisNum = 0.0;
  for ( size_t i = 0; i < N; i++ ) {
    size_t visNodeNum = 0;
    parlay::sequence<coord> array_queue( K );
    kBoundedQueue<coord> bq( array_queue.cut( 0, K ) );
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

  return;
}

template<typename point>
void
testParallelKDtree( const int& Dim, const int& LEAVE_WRAP, parlay::sequence<point>& wp,
                    const size_t& N, const int& K, const int& rounds,
                    const string& insertFile, const int& tag ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using interior = typename tree::interior;
  using leaf = typename tree::leaf;
  using node_tag = typename tree::node_tag;
  using node_tags = typename tree::node_tags;

  if ( N != wp.size() ) {
    puts( "input parameter N is different to input points size" );
    abort();
  }

  tree pkd;
  points wi;
  Typename* kdknn;

  //* begin test
  buildTree<point>( Dim, wp, rounds, pkd );

  //* batch insert
  if ( tag >= 1 ) {
    auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
    if ( nd != Dim ) {
      puts( "read inserted points dimension wrong" );
      abort();
    }

    batchInsert<point>( pkd, wp, wi, Dim, rounds );

    wp.append( wi );
    kdknn = new Typename[N + wi.size()];
  } else {
    kdknn = new Typename[N];
    std::cout << "-1 " << std::flush;
  }

  //* batch delete
  if ( tag >= 2 ) {
  } else {
    std::cout << "-1 " << std::flush;
  }

  // todo update size of kdknn in the end
  queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K );

  std::cout << std::endl;
  return;
}

int
main( int argc, char* argv[] ) {
  commandLine P( argc, argv,
                 "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                 "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-i <_insertFile>]" );
  char* iFile = P.getOptionValue( "-p" );
  char* _insertFile = P.getOptionValue( "-i" );
  int K = P.getOptionIntValue( "-k", 100 );
  int Dim = P.getOptionIntValue( "-d", 3 );
  size_t N = P.getOptionLongValue( "-n", -1 );
  int tag = P.getOptionIntValue( "-t", 1 );
  int rounds = P.getOptionIntValue( "-r", 3 );

  int LEAVE_WRAP = 32;
  parlay::sequence<point15D> wp;
  std::string name, insertFile;

  //* initialize points
  if ( iFile != NULL ) {
    name = std::string( iFile );
    name = name.substr( name.rfind( "/" ) + 1 );
    std::cout << name << " ";
    auto [n, d] = read_points<point15D>( iFile, wp, K );
    N = n;
    assert( d == Dim );
  } else {  //* construct data byself
    K = 100;
    generate_random_points<point15D>( wp, 1000000, N, Dim );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  if ( tag == 1 ) {
    if ( _insertFile == NULL ) {
      int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
      id = ( id + 1 ) % 3;  //! MOD graph number used to test
      if ( !id ) id++;
      int pos = std::string( iFile ).rfind( "/" ) + 1;
      insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
    } else {
      insertFile = std::string( _insertFile );
    }
  }

  assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

  if ( tag == -1 ) {
    //* serial run
    // todo rewrite test serial code
    // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
  } else if ( Dim == 2 ) {
    auto pts = parlay::tabulate(
        N, [&]( size_t i ) -> point2D { return point2D( wp[i].pnt.begin() ); } );
    decltype( wp )().swap( wp );
    testParallelKDtree<point2D>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag );
  } else if ( Dim == 3 ) {
    auto pts = parlay::tabulate(
        N, [&]( size_t i ) -> point3D { return point3D( wp[i].pnt.begin() ); } );
    decltype( wp )().swap( wp );
    testParallelKDtree<point3D>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag );
  } else if ( Dim == 5 ) {
    auto pts = parlay::tabulate(
        N, [&]( size_t i ) -> point5D { return point5D( wp[i].pnt.begin() ); } );
    decltype( wp )().swap( wp );
    testParallelKDtree<point5D>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag );
  } else if ( Dim == 7 ) {
    auto pts = parlay::tabulate(
        N, [&]( size_t i ) -> point7D { return point7D( wp[i].pnt.begin() ); } );
    decltype( wp )().swap( wp );
    testParallelKDtree<point7D>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag );
  }

  return 0;
}
