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
                    const string& insertFile, const int& tag, const int& queryType ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using interior = typename tree::interior;
  using leaf = typename tree::leaf;
  using node_tag = typename tree::node_tag;
  using node_tags = typename tree::node_tags;
  using box = typename tree::box;

  if ( N != wp.size() ) {
    puts( "input parameter N is different to input points size" );
    abort();
  }

  tree pkd;

  points wi;
  if ( tag >= 1 ) {
    auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
    if ( nd != Dim ) {
      puts( "read inserted points dimension wrong" );
      abort();
    }
  }

  Typename* kdknn;

  //* begin test
  buildTree<point>( Dim, wp, rounds, pkd );

  //* batch insert
  if ( tag >= 1 ) {

    batchInsert<point>( pkd, wp, wi, Dim, rounds );

    if ( tag == 1 ) {
      wp.append( wi );
    }
  } else {
    std::cout << "-1 " << std::flush;
  }

  //* batch delete
  if ( tag >= 2 ) {
    assert( wi.size() );
    batchDelete<point>( pkd, wp, wi, Dim, rounds );
  } else {
    std::cout << "-1 " << std::flush;
  }

  kdknn = new Typename[wp.size()];
  int queryNum = 1000;

  if ( queryType & ( 1 << 0 ) ) {
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );
  }

  if ( queryType & ( 1 << 1 ) ) {
    rangeCount<point>( wp, pkd, kdknn, rounds, queryNum );
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 2 ) ) {
    if ( !( queryType & ( 1 << 1 ) ) ) {  //* run range count to obtain max candidate size
      parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
        box queryBox = pkd.get_box( box( wp[i], wp[i] ),
                                    box( wp[( i + wp.size() / 2 ) % wp.size()],
                                         wp[( i + wp.size() / 2 ) % wp.size()] ) );
        kdknn[i] = pkd.range_count( queryBox );
      } );
    }
    //* reduce
    auto maxReduceSize = parlay::reduce(
        parlay::delayed_tabulate( queryNum, [&]( size_t i ) { return kdknn[i]; } ),
        parlay::maximum<Typename>() );
    points Out( queryNum * maxReduceSize );
    rangeQuery<point>( wp, pkd, kdknn, rounds, queryNum, Out );
  } else {
    std::cout << "-1 " << std::flush;
  }

  std::cout << std::endl;
  return;
}

int
main( int argc, char* argv[] ) {
  commandLine P(
      argc, argv,
      "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
      "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i <_insertFile>]" );
  char* iFile = P.getOptionValue( "-p" );
  char* _insertFile = P.getOptionValue( "-i" );
  int K = P.getOptionIntValue( "-k", 100 );
  int Dim = P.getOptionIntValue( "-d", 3 );
  size_t N = P.getOptionLongValue( "-n", -1 );
  int tag = P.getOptionIntValue( "-t", 1 );
  int rounds = P.getOptionIntValue( "-r", 3 );
  int queryType = P.getOptionIntValue( "-q", 0 );

  int LEAVE_WRAP = 32;
  parlay::sequence<PointType<coord, 15>> wp;
  std::string name, insertFile;

  //* initialize points
  if ( iFile != NULL ) {
    name = std::string( iFile );
    name = name.substr( name.rfind( "/" ) + 1 );
    std::cout << name << " ";
    auto [n, d] = read_points<PointType<coord, 15>>( iFile, wp, K );
    N = n;
    assert( d == Dim );
  } else {  //* construct data byself
    K = 100;
    generate_random_points<PointType<coord, 15>>( wp, 1000000, N, Dim );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  if ( tag >= 1 ) {
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
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<long, 2> {
      return PointType<long, 2>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<long, 2>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                            insertFile, tag, queryType );
  } else if ( Dim == 3 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<long, 3> {
      return PointType<long, 3>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<long, 3>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                            insertFile, tag, queryType );
  } else if ( Dim == 5 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<long, 5> {
      return PointType<long, 5>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<long, 5>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                            insertFile, tag, queryType );
  } else if ( Dim == 7 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<long, 7> {
      return PointType<long, 7>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<long, 7>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                            insertFile, tag, queryType );
  }

  return 0;
}
