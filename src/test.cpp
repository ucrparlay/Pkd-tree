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

  return;
}

template<typename point>
void testParallelKDtree( const int& Dim, const int& LEAVE_WRAP,
                         parlay::sequence<point>& wp, const size_t& N, const int& K,
                         const int& rounds, const string& insertFile, const int& tag ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using interior = typename tree::interior;
  using leaf = typename tree::leaf;
  using node_tag = typename tree::node_tag;
  using node_tags = typename tree::node_tags;
  assert( N == wp.size() );
  tree pkd;

  buildTree( Dim, wp, rounds, pkd );

  points wi = points::uninitialized( N );
  Typename* kdknn;
  if ( tag == 1 ) {
    size_t tmpn;
    int tmpd;
    read_points<point>( insertFile.c_str(), wi, tmpn, tmpd, K );
    assert( wi.size() == N && tmpn == N && tmpd == Dim );
    size_t len = ( N / 2 ) / rounds;

    batchInsert( pkd, wi, Dim, rounds, len );

    wp.append( wi );
    kdknn = new Typename[N + N / 2 + 1];
  } else {
    kdknn = new Typename[N];
  }

  queryKNN( Dim, wp, rounds, pkd, kdknn, K );
  puts( "" );
  return;
}

int main( int argc, char* argv[] ) {
  commandLine P( argc, argv,
                 "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                 "<parallelTag>] [-p <inFile>] [-r {1,...,5}]" );
  char* iFile = P.getOptionValue( "-p" );
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
    read_points<point15D>( iFile, wp, N, Dim, K );
  } else {  //* construct data byself
    K = 100;
    generate_random_points<point15D>( wp, 1000000, N, Dim );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  if ( tag == 1 ) {
    int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
    id = ( id + 1 ) % 3;  //! MOD graph number used to test
    if ( !id ) id++;
    int pos = std::string( iFile ).rfind( "/" ) + 1;
    insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
    LOG << insertFile << ENDL;
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
