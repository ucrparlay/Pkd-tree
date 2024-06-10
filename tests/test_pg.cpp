#include <initializer_list>
#include "testFramework_pg.h"

template<class TreeDesc, typename point>
void
testParallelKDtree( const int& Dim, const int& LEAVE_WRAP, parlay::sequence<point>& wp,
                    const size_t& N, const int& K, const int& rounds,
                    const string& insertFile, const int& tag_ext, const int& queryType ) {
  // using tree = ParallelKDtree<point>;
  constexpr const int DimMax = point::dim;
  using points = parlay::sequence<point>;
  // using node = pargeo::kdTree::node<DimMax, point>;
  // using interior = typename tree::interior;
  // using leaf = typename tree::leaf;
  // using node_tag = typename tree::node_tag;
  // using node_tags = typename tree::node_tags;
  // using box = typename tree::box;

  int tag = tag_ext & 0xf;
  int ins_ratio = ( tag_ext >> 4 ) & 0xf;
  int downsize_k = ( tag_ext >> 8 ) & 0xf;
  int seg_mode = ( tag_ext >> 12 ) & 0xf;
  const int build_mode = ( tag_ext >> 16 ) & 0xf;
  const int ins_mode = ( tag_ext >> 20 ) & 0xf;
  const int del_mode = ( tag_ext >> 24 ) & 0xf;
  printf( "tag=%d ins_ratio=%d\n", tag, ins_ratio );
  printf( "downsize_k=%d seg_mode=%d\n", downsize_k, seg_mode );
  printf( "build_mode=%d\n", build_mode );
  printf( "ins_mode=%d del_mode=%d\n", ins_mode, del_mode );

  if ( N != wp.size() ) {
    puts( "input parameter N is different to input points size" );
    abort();
  }

  points wi;
  /*if ( tag >= 1 ) {*/
  /*  auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );*/
  /*  if ( nd != Dim ) {*/
  /*    puts( "read inserted points dimension wrong" );*/
  /*    abort();*/
  /*  }*/
  /*}*/

  Typename* kdknn;
  /*return;*/

  //* begin test
  using Tree = typename TreeDesc::type;
  Tree* pkd = nullptr;
  if ( build_mode == 0 )  // unit test
  {
    pkd = buildTree<TreeDesc, point>( Dim, wp, rounds, LEAVE_WRAP );
  } else if ( build_mode == 1 )  // build from wp
  {
    const auto& cwp = wp;
    pkd = new Tree( parlay::make_slice( cwp ) );
  } else if ( build_mode == 2 )  // build from empty (wp will be inserted later)
  {
    wi = wp;
    const int log2size = (int)std::ceil( std::log2( wi.size() ) );
    pkd = new Tree( log2size );
  }

  std::cout << std::endl;
  return;
}

template<class TreeDesc, typename point>
void
bench_osm_year( int Dim, int LEAVE_WRAP, int K, int rounds, const string& osm_prefix,
                int tag_ext ) {
  const uint32_t year_begin = ( tag_ext >> 4 ) & 0xfff;
  const uint32_t year_end = ( tag_ext >> 16 ) & 0xfff;
  printf( "year: %u - %u\n", year_begin, year_end );

  using points = parlay::sequence<point>;
  parlay::sequence<points> node_by_year( year_end - year_begin + 1 );
  for ( uint32_t year = year_begin; year <= year_end; ++year ) {
    const std::string path = osm_prefix + "osm_" + std::to_string( year ) + ".csv";
    printf( "read from %s\n", path.c_str() );
    read_points<point>( path.c_str(), node_by_year[year - year_begin], K );
  }

  // NOTICE: we fix the query size 10^7
  points qs;
  for ( const auto& ps : node_by_year ) {
    if ( qs.size() == 10000000 ) break;
    qs.insert( qs.end(), ps.begin(),
               ps.begin() + std::min( 10000000 - qs.size(), ps.size() ) );
    printf( "inc qs size to %lu\n", qs.size() );
  }
  assert( qs.size() == 10000000 );
  printf( "qs size: %lu\n", qs.size() );

  using Tree = typename TreeDesc::type;
  Tree* pkd = nullptr;
  insertOsmByTime<TreeDesc, point>( Dim, node_by_year, rounds, pkd, qs, K );
}

template<class TreeDesc, typename point>
void
bench_osm_month( int Dim, int LEAVE_WRAP, int K, int rounds, const string& osm_prefix,
                 int tag_ext ) {
  const uint32_t year_begin = ( tag_ext >> 4 ) & 0xfff;
  const uint32_t year_end = ( tag_ext >> 16 ) & 0xfff;
  printf( "year: %u - %u\n", year_begin, year_end );

  using points = parlay::sequence<point>;
  parlay::sequence<points> node( ( year_end - year_begin + 1 ) * 12 );
  for ( uint32_t year = year_begin; year <= year_end; ++year )
    for ( uint32_t month = 1; month <= 12; ++month ) {
      const std::string path =
          osm_prefix + std::to_string( year ) + "/" + std::to_string( month ) + ".csv";
      printf( "read from %s\n", path.c_str() );
      read_points<point>( path.c_str(), node[( year - year_begin ) * 12 + month - 1], K );
    }

  // NOTICE: we fix the query size 10^7
  points qs;
  for ( const auto& ps : node ) {
    if ( qs.size() == 10000000 ) break;
    qs.insert( qs.end(), ps.begin(),
               ps.begin() + std::min( 10000000 - qs.size(), ps.size() ) );
    printf( "inc qs size to %lu\n", qs.size() );
  }
  assert( qs.size() == 10000000 );
  printf( "qs size: %lu\n", qs.size() );

  using Tree = typename TreeDesc::type;
  Tree* pkd = nullptr;
  insertOsmByTime<TreeDesc, point>( Dim, node, rounds, pkd, qs, K );
}

template<typename T, uint_fast8_t d>
// using PointType = pargeo::_point<d,T,double,pargeo::_empty>;
using PointType = pargeo::batchKdTree::point<d>;

struct wrapper {
  constexpr const static bool parallel = true;
  constexpr const static bool coarsen = true;

  constexpr const static int NUM_TREES = 24;
  constexpr const static int BUFFER_LOG2_SIZE = 7;

  struct COTree_t {
    template<class Point>
    struct desc {
      using type = pargeo::batchKdTree::CO_KdTree<Point::dim, Point, parallel, coarsen>;
      constexpr static const bool support_knn1 = true;
      constexpr static const bool support_knn2 = false;
      constexpr static const bool support_knn3 = false;
      constexpr static const bool support_insert = false;
      constexpr static const bool support_insert_delete = false;
    };
  };
  struct BHLTree_t {
    template<class Point>
    struct desc {
      using type = pargeo::batchKdTree::BHL_KdTree<Point::dim, Point, parallel, coarsen>;
      constexpr static const bool support_knn1 = true;
      constexpr static const bool support_knn2 = false;
      constexpr static const bool support_knn3 = false;
      constexpr static const bool support_insert = true;
      constexpr static const bool support_insert_delete = true;
    };
  };
  struct LogTree_t {
    template<class Point>
    struct desc {
      using type = pargeo::batchKdTree::LogTree<NUM_TREES, BUFFER_LOG2_SIZE, Point::dim,
                                                Point, parallel, coarsen>;
      constexpr static const bool support_knn1 = false;
      constexpr static const bool support_knn2 = false;
      constexpr static const bool support_knn3 = true;
      constexpr static const bool support_insert = true;
      constexpr static const bool support_insert_delete = true;
    };
  };
};

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
  int treeType = P.getOptionIntValue( "-T", 0 );

  pargeo::batchKdTree::print_config();

  int LEAVE_WRAP = 32;
  parlay::sequence<PointType<coord, 3>> wp;
  std::string name, insertFile;

  //* initialize points
  if ( iFile != NULL ) {
    name = std::string( iFile );
    name = name.substr( name.rfind( "/" ) + 1 );
    std::cout << name << " ";
    auto [n, d] = read_points<PointType<coord, 3>>( iFile, wp, K );
    N = n;
    assert( d == Dim );
  }

  /*if ( ( tag & 0xf ) >= 1 ) {*/
  /*  if ( _insertFile == NULL ) {*/
  /*    int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );*/
  /*    if ( Dim != 2 ) id = ( id + 1 ) % 3;  //! MOD graph number used to test*/
  /*    if ( !id ) id++;*/
  /*    int pos = std::string( iFile ).rfind( "/" ) + 1;*/
  /*    insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) +
   * ".in";*/
  /*  } else {*/
  /*    insertFile = std::string( _insertFile );*/
  /*  }*/
  /*}*/

  assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

  auto run_test = [&]<class Wrapper>( Wrapper ) {
    auto run = [&]( auto dim_wrapper ) {
      constexpr const auto D = decltype( dim_wrapper )::value;
      using point_t = PointType<coord, D>;
      using Desc = typename Wrapper::template desc<point_t>;

      /*if ( ( tag & 0x40000000 ) && ( tag & 0xf ) == 1 ) {*/
      /*  bench_osm_year<Desc, point_t>( Dim, LEAVE_WRAP, K, rounds, insertFile, tag );*/
      /*  return;*/
      /*}*/
      /*if ( ( tag & 0x40000000 ) && ( tag & 0xf ) == 2 ) {*/
      /*  bench_osm_month<Desc, point_t>( Dim, LEAVE_WRAP, K, rounds, insertFile, tag );*/
      /*  return;*/
      /*}*/
      /*auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, D> {*/
      /*  return PointType<coord, D>( wp[i].coordinate() );*/
      /*} );*/
      /*decltype( wp )().swap( wp );*/
      /*testParallelKDtree<Desc>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag,*/
      /*                          queryType );*/
      testParallelKDtree<Desc>( Dim, LEAVE_WRAP, wp, N, K, rounds, insertFile, tag,
                                queryType );
    };

    /*if ( tag == -1 ) {*/
    /*  //* serial run*/
    /*  // todo rewrite test serial code*/
    /*  // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );*/
    /*} else if ( Dim == 2 ) {*/
    /*  run( std::integral_constant<int, 2>{} );*/
    /*} else if ( Dim == 3 ) {*/
    run( std::integral_constant<int, 3>{} );
    /*} else if ( Dim == 5 ) {*/
    /*  run( std::integral_constant<int, 5>{} );*/
    /*} else if ( Dim == 7 ) {*/
    /*  run( std::integral_constant<int, 7>{} );*/
    /*} else if ( Dim == 9 ) {*/
    /*  run( std::integral_constant<int, 9>{} );*/
    /*} else if ( Dim == 10 ) {*/
    /*  run( std::integral_constant<int, 10>{} );*/
    /*}*/
  };

  if ( treeType == 0 )
    run_test( wrapper::COTree_t{} );
  else if ( treeType == 1 )
    run_test( wrapper::BHLTree_t{} );
  else if ( treeType == 2 )
    run_test( wrapper::LogTree_t{} );

  return 0;
}
