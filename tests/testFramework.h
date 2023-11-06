#pragma once

#include "cpdd/cpdd.h"

#include "common/geometryIO.h"
#include "common/parse_command_line.h"
#include "common/time_loop.h"

using coord = long;
using Typename = coord;
using namespace cpdd;

//*---------- generate points within a 0-box_size --------------------
template<typename point>
void
generate_random_points( parlay::sequence<point>& wp, coord _box_size, long n, int Dim ) {
  coord box_size = _box_size;

  std::random_device rd;        // a seed source for the random number engine
  std::mt19937 gen_mt( rd() );  // mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<int> distrib( 1, box_size );

  parlay::random_generator gen( distrib( gen_mt ) );
  std::uniform_int_distribution<int> dis( 0, box_size );

  wp.resize( n );
  // generate n random points in a cube
  parlay::parallel_for(
      0, n,
      [&]( long i ) {
        auto r = gen[i];
        for ( int j = 0; j < Dim; j++ ) {
          wp[i].pnt[j] = dis( r );
        }
      },
      1000 );
  return;
}

template<typename point>
std::pair<size_t, int>
read_points( const char* iFile, parlay::sequence<point>& wp, int K,
             bool withID = false ) {
  using coord = typename point::coord;
  using coords = typename point::coords;
  static coords samplePoint;
  parlay::sequence<char> S = readStringFromFile( iFile );
  parlay::sequence<char*> W = stringToWords( S );
  size_t N = atol( W[0] );
  int Dim = atoi( W[1] );
  assert( N >= 0 && Dim >= 1 && N >= K );

  auto pts = W.cut( 2, W.size() );
  assert( pts.size() % Dim == 0 );
  size_t n = pts.size() / Dim;
  auto a = parlay::tabulate( Dim * n, [&]( size_t i ) -> coord {
    if constexpr ( std::is_integral_v<coord> )
      return atol( pts[i] );
    else if ( std::is_floating_point_v<coord> )
      return atof( pts[i] );
  } );
  wp.resize( N );
  parlay::parallel_for( 0, n, [&]( size_t i ) {
    for ( int j = 0; j < Dim; j++ ) {
      wp[i].pnt[j] = a[i * Dim + j];
      if constexpr ( std::is_same_v<point, PointType<coord, samplePoint.size()>> ) {
      } else {
        wp[i].id = i;
      }
    }
  } );
  return std::make_pair( N, Dim );
}

//* [a,b)
size_t
get_random_index( size_t a, size_t b ) {
  return size_t( ( rand() % ( b - a ) ) + a );
}

template<typename point>
void
recurse_box( parlay::slice<point*, point*> In,
             parlay::sequence<std::pair<point, point>>& boxs, int DIM,
             std::pair<size_t, size_t> range, int& idx, int recNum, bool first ) {
  using tree = ParallelKDtree<point>;
  using box = typename tree::box;

  size_t n = In.size();
  if ( idx >= recNum || n < range.first || n == 0 ) return;

  if ( !first && n >= range.first && n <= range.second ) {
    // LOG << n << ENDL;
    boxs[idx++] = tree::get_box( In );
    return;
  }

  int dim = get_random_index( 0, DIM );
  size_t pos = get_random_index( 0, n );
  parlay::sequence<bool> flag( n, 0 );
  parlay::parallel_for( 0, n, [&]( size_t i ) {
    if ( cpdd::Num_Comparator<coord>::Gt( In[i].pnt[dim], In[pos].pnt[dim] ) )
      flag[i] = 1;
    else
      flag[i] = 0;
  } );
  auto [Out, m] = parlay::internal::split_two( In, flag );

  assert( Out.size() == n );
  // LOG << dim << " " << Out[0] << Out[m] << ENDL;

  parlay::par_do_if(
      0,
      [&]() { recurse_box<point>( Out.cut( 0, m ), boxs, DIM, range, idx, recNum, 0 ); },
      [&]() {
        recurse_box<point>( Out.cut( m, n ), boxs, DIM, range, idx, recNum, 0 );
      } );

  return;
}

template<typename point>
parlay::sequence<std::pair<point, point>>
gen_rectangles( int recNum, int type, const parlay::sequence<point>& WP, int DIM ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using box = typename tree::box;
  using boxs = parlay::sequence<box>;

  size_t n = WP.size();
  std::pair<size_t, size_t> range;
  if ( type == 0 ) {  //* small bracket
    range.first = std::numeric_limits<size_t>::min();
    range.second = size_t( std::sqrt( std::sqrt( n ) ) );
  } else if ( type == 1 ) {  //* medium bracket
    range.first = size_t( std::sqrt( std::sqrt( n ) ) );
    range.second = size_t( std::sqrt( n ) );
  } else {  //* large bracket
    range.first = size_t( std::sqrt( n ) );
    range.second = std::numeric_limits<size_t>::max();
  }

  boxs bxs( recNum );
  int cnt = 0;
  points wp( n );

  srand( 10 );

  while ( cnt < recNum ) {
    parlay::copy( WP, wp );
    recurse_box<point>( parlay::make_slice( wp ), bxs, DIM, range, cnt, recNum, 1 );
  }

  return std::move( bxs );
}

template<typename tree>
void
checkTreeSameSequential( typename tree::node* T, int dim, const int& DIM ) {
  if ( T->is_leaf ) {
    assert( T->dim == dim );
    return;
  }
  typename tree::interior* TI = static_cast<typename tree::interior*>( T );
  assert( TI->split.second == dim && TI->dim == dim );
  dim = ( dim + 1 ) % DIM;
  parlay::par_do_if(
      T->size > 1000, [&]() { checkTreeSameSequential<tree>( TI->left, dim, DIM ); },
      [&]() { checkTreeSameSequential<tree>( TI->right, dim, DIM ); } );
  return;
}

template<typename tree>
size_t
checkTreesSize( typename tree::node* T ) {
  if ( T->is_leaf ) {
    return T->size;
  }
  typename tree::interior* TI = static_cast<typename tree::interior*>( T );
  size_t l = checkTreesSize<tree>( TI->left );
  size_t r = checkTreesSize<tree>( TI->right );
  assert( l + r == T->size );
  return T->size;
}

template<typename point>
void
buildTree( const int& Dim, const parlay::sequence<point>& WP, const int& rounds,
           ParallelKDtree<point>& pkd ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;

  size_t n = WP.size();
  // points wp = points::uninitialized( n );
  points wp = points( n );

  double aveBuild = time_loop(
      rounds, 1.0, [&]() { parlay::copy( WP.cut( 0, n ), wp.cut( 0, n ) ); },
      [&]() { pkd.build( wp.cut( 0, n ), Dim ); }, [&]() { pkd.delete_tree(); } );

  LOG << aveBuild << " " << std::flush;

  //* return a built tree
  parlay::sequence<coord> p = { 48399, 65087, 66178, 74404, 2991 };

  parlay::copy( WP.cut( 0, n ), wp.cut( 0, n ) );
  pkd.build( wp.cut( 0, n ), Dim );
  // pkd.delete_tree();
  return;
}

template<typename point>
void
batchInsert( ParallelKDtree<point>& pkd, const parlay::sequence<point>& WP,
             const parlay::sequence<point>& WI, const uint_fast8_t& DIM,
             const int& rounds, double ratio = 1.0 ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  points wp = points::uninitialized( WP.size() );
  points wi = points::uninitialized( WI.size() );

  pkd.delete_tree();

  double aveInsert = time_loop(
      rounds, 1.0,
      [&]() {
        parlay::copy( WP, wp ), parlay::copy( WI, wi );
        pkd.build( parlay::make_slice( wp ), DIM );
      },
      [&]() { pkd.batchInsert( wi.cut( 0, size_t( wi.size() * ratio ) ), DIM ); },
      [&]() { pkd.delete_tree(); } );

  //* set status to be finish insert
  parlay::copy( WP, wp ), parlay::copy( WI, wi );
  pkd.build( parlay::make_slice( wp ), DIM );
  pkd.batchInsert( wi.cut( 0, size_t( wi.size() * ratio ) ), DIM );

  LOG << aveInsert << " " << std::flush;

  return;
}

template<typename point>
void
batchDelete( ParallelKDtree<point>& pkd, const parlay::sequence<point>& WP,
             const parlay::sequence<point>& WI, const uint_fast8_t& DIM,
             const int& rounds, bool afterInsert = 1, double ratio = 1.0 ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  points wp = points::uninitialized( WP.size() );
  points wi =
      points::uninitialized( WP.size() );  //! warnning need to adjust space if necessary

  pkd.delete_tree();

  double aveDelete = time_loop(
      rounds, 1.0,
      [&]() {
        if ( afterInsert ) {  //* first insert wi then delete wi
          parlay::copy( WP, wp ), parlay::copy( WI, wi );
          pkd.build( parlay::make_slice( wp ), DIM );
          pkd.batchInsert( parlay::make_slice( wi ), DIM );
          parlay::copy( WP, wp ), parlay::copy( WI, wi );
        } else {  //* only build wp and then delete
          parlay::copy( WP, wp ), parlay::copy( WP, wi );
          pkd.build( parlay::make_slice( wp ), DIM );
        }
      },
      [&]() { pkd.batchDelete( wi.cut( 0, size_t( wi.size() * ratio ) ), DIM ); },
      [&]() { pkd.delete_tree(); } );

  //* set status to be finish delete

  if ( afterInsert ) {
    parlay::copy( WP, wp ), parlay::copy( WI, wi );
    pkd.build( parlay::make_slice( wp ), DIM );
    pkd.batchInsert( parlay::make_slice( wi ), DIM );
    parlay::copy( WP, wp ), parlay::copy( WI, wi );
    pkd.batchDelete( parlay::make_slice( wi ), DIM );
  } else {
    parlay::copy( WP, wp ), parlay::copy( WP, wi );
    pkd.build( parlay::make_slice( wp ), DIM );
    pkd.batchDelete( wi.cut( 0, size_t( wi.size() * ratio ) ), DIM );
  }

  std::cout << aveDelete << " " << std::flush;

  return;
}

template<typename point>
void
queryKNN( const uint_fast8_t& Dim, const parlay::sequence<point>& WP, const int& rounds,
          ParallelKDtree<point>& pkd, Typename* kdknn, const int K,
          const bool checkCorrect ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using coord = typename point::coord;
  using nn_pair = std::pair<std::reference_wrapper<point>, coord>;
  // using nn_pair = std::pair<point, coord>;
  size_t n = WP.size();
  int LEAVE_WRAP = 32;
  node* KDParallelRoot = pkd.get_root();
  points wp = points::uninitialized( n );
  parlay::copy( WP, wp );

  parlay::sequence<nn_pair> Out( K * n, nn_pair( std::ref( wp[0] ), 0 ) );
  parlay::sequence<kBoundedQueue<point, nn_pair>> bq =
      parlay::sequence<kBoundedQueue<point, nn_pair>>::uninitialized( n );
  parlay::parallel_for(
      0, n, [&]( size_t i ) { bq[i].resize( Out.cut( i * K, i * K + K ) ); } );
  parlay::sequence<size_t> visNum( n );

  // size_t visNodeNum = 0;
  // pkd.k_nearest( KDParallelRoot, wp[0], Dim, bq[0], visNodeNum );
  // int deep = pkd.getTreeHeight( pkd.get_root(), 0 );
  // LOG << deep << " " << visNodeNum << ENDL;

  double aveQuery = time_loop(
      rounds, 1.0,
      [&]() { parlay::parallel_for( 0, n, [&]( size_t i ) { bq[i].reset(); } ); },
      [&]() {
        if ( !checkCorrect ) {  //! Ensure pkd.size() == wp.size()
          pkd.flatten( pkd.get_root(), parlay::make_slice( wp ) );
        }
        double aveVisNum = 0.0;
        parlay::parallel_for( 0, n, [&]( size_t i ) {
          size_t visNodeNum = 0;
          pkd.k_nearest( KDParallelRoot, wp[i], Dim, bq[i], visNodeNum );
          kdknn[i] = bq[i].top().second;
          // visNum[i] = ( 1.0 * visNodeNum ) / n;
          visNum[i] = visNodeNum;
        } );
      },
      [&]() {} );

  LOG << aveQuery << " " << std::flush;

  int deep = pkd.getTreeHeight( pkd.get_root(), 0 );
  // parlay::sequence<int> heights( n );
  // int idx = 0;
  // pkd.countTreeHeights( pkd.get_root(), 0, idx, heights );
  // auto kv = parlay::histogram_by_key( heights.cut( 0, idx ) );
  // std::sort( kv.begin(), kv.end(), [&]( auto a, auto b ) { return a.first < b.first; }
  // ); puts( "--" ); for ( auto i : kv ) {
  //   LOG << i.first << " " << i.second << ENDL;
  // }
  LOG << deep << " " << parlay::reduce( visNum.cut( 0, n ) ) / n << " " << std::flush;

  return;
}

template<typename point>
void
rangeCount( const parlay::sequence<point>& wp, ParallelKDtree<point>& pkd,
            Typename* kdknn, const int& rounds, const int& queryNum ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using box = typename tree::box;

  int n = wp.size();

  double aveCount = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
          box queryBox = pkd.get_box(
              box( wp[i], wp[i] ), box( wp[( i + n / 2 ) % n], wp[( i + n / 2 ) % n] ) );
          kdknn[i] = pkd.range_count( queryBox );
        } );
      },
      [&]() {} );

  LOG << aveCount << " " << std::flush;

  return;
}

template<typename point>
void
rangeQuery( const parlay::sequence<point>& wp, ParallelKDtree<point>& pkd,
            Typename* kdknn, const int& rounds, const int& queryNum,
            parlay::sequence<point>& Out ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using box = typename tree::box;

  int n = wp.size();
  size_t step = Out.size() / queryNum;
  using ref_t = std::reference_wrapper<point>;
  parlay::sequence<ref_t> out_ref( Out.size(), std::ref( Out[0] ) );

  double aveQuery = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
          box queryBox = pkd.get_box(
              box( wp[i], wp[i] ), box( wp[( i + n / 2 ) % n], wp[( i + n / 2 ) % n] ) );
          kdknn[i] =
              pkd.range_query( queryBox, out_ref.cut( i * step, ( i + 1 ) * step ) );
        } );
      },
      [&]() {} );

  parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
    for ( int j = 0; j < kdknn[i]; j++ ) {
      Out[i * step + j] = out_ref[i * step + j];
    }
  } );

  LOG << aveQuery << " " << std::flush;
  return;
}

//* test range count for fix rectangle
template<typename point>
void
rangeCountFix( const parlay::sequence<point>& WP, ParallelKDtree<point>& pkd,
               Typename* kdknn, const int& rounds, int recType, int recNum, int DIM ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using box = typename tree::box;

  int n = WP.size();

  auto queryBox = gen_rectangles( recNum, recType, WP, DIM );

  double aveCount = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for(
            0, recNum, [&]( size_t i ) { kdknn[i] = pkd.range_count( queryBox[i] ); } );
      },
      [&]() {} );

  LOG << aveCount << " " << std::flush;

  return;
}

//* test range query for fix rectangle
template<typename point>
void
rangeQueryFix( const parlay::sequence<point>& WP, ParallelKDtree<point>& pkd,
               Typename* kdknn, const int& rounds, parlay::sequence<point>& Out,
               int recType, int recNum, int DIM ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using box = typename tree::box;

  auto queryBox = gen_rectangles( recNum, recType, WP, DIM );

  int n = WP.size();
  size_t step = Out.size() / recNum;
  using ref_t = std::reference_wrapper<point>;
  parlay::sequence<ref_t> out_ref( Out.size(), std::ref( Out[0] ) );

  double aveQuery = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for( 0, recNum, [&]( size_t i ) {
          kdknn[i] =
              pkd.range_query( queryBox[i], out_ref.cut( i * step, ( i + 1 ) * step ) );
        } );
      },
      [&]() {} );

  LOG << aveQuery << " " << std::flush;
  return;
}

template<typename point>
void
generate_knn( const uint_fast8_t& Dim, const parlay::sequence<point>& WP, const int K,
              const char* outFile ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using coord = typename point::coord;
  using nn_pair = std::pair<point, coord>;
  // using nn_pair = std::pair<std::reference_wrapper<point>, coord>;
  using ID_type = uint;

  size_t n = WP.size();

  tree pkd;
  points wp = points( n );
  parlay::copy( WP.cut( 0, n ), wp.cut( 0, n ) );

  pkd.build( parlay::make_slice( wp ), Dim );

  parlay::sequence<nn_pair> Out( K * n, nn_pair( std::ref( wp[0] ), 0 ) );
  parlay::sequence<kBoundedQueue<point, nn_pair>> bq =
      parlay::sequence<kBoundedQueue<point, nn_pair>>::uninitialized( n );
  parlay::parallel_for(
      0, n, [&]( size_t i ) { bq[i].resize( Out.cut( i * K, i * K + K ) ); } );

  node* KDParallelRoot = pkd.get_root();
  parlay::parallel_for( 0, n, [&]( size_t i ) { bq[i].reset(); } );

  std::cout << "begin query" << std::endl;
  parlay::parallel_for( 0, n, [&]( size_t i ) {
    size_t visNodeNum = 0;
    pkd.k_nearest( KDParallelRoot, WP[i], Dim, bq[i], visNodeNum );
  } );

  std::cout << "finish query" << std::endl;

  std::ofstream ofs( outFile );
  if ( !ofs.is_open() ) {
    throw( "file not open" );
    abort();
  }
  size_t m = n * K;
  ofs << "WeightedAdjacencyGraph" << '\n';
  ofs << n << '\n';
  ofs << m << '\n';
  parlay::sequence<uint64_t> offset( n + 1 );
  parlay::parallel_for( 0, n + 1, [&]( size_t i ) { offset[i] = i * K; } );
  // parlay::parallel_for( 0, n, [&]( size_t i ) {
  //   for ( size_t j = 0; j < K; j++ ) {
  //     if ( Out[i * K + j].first == wp[i] ) {
  //       printf( "%d, self-loop\n", i );
  //       exit( 0 );
  //     }
  //   }
  // } );

  parlay::sequence<ID_type> edge( m );
  parlay::parallel_for( 0, m, [&]( size_t i ) { edge[i] = Out[i].first.id; } );
  parlay::sequence<double> weight( m );
  parlay::parallel_for( 0, m, [&]( size_t i ) { weight[i] = Out[i].second; } );
  for ( size_t i = 0; i < n; i++ ) {
    ofs << offset[i] << '\n';
  }
  for ( size_t i = 0; i < m; i++ ) {
    ofs << edge[i];
    ofs << "\n";
    // ofs << edge[i] << '\n';
  }
  for ( size_t i = 0; i < m; i++ ) {
    ofs << weight[i] << '\n';
  }
  ofs.close();
  return;
}