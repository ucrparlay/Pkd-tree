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
  if ( tag >= 1 ) {
    auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
    if ( nd != Dim ) {
      puts( "read inserted points dimension wrong" );
      abort();
    }
  }

  Typename* kdknn;

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

  if ( tag == 3 ) {
    const auto ins_size = size_t( wi.size() ) * ins_ratio / 10;
    parlay::sequence<point> pwi( wi.begin(), wi.begin() + ins_size );
    batchInsertDelete<TreeDesc, point>( pkd, pwi, Dim, 1 );
    tag = 0;
  }

  const int seg_ratio[] = { 100, 10, 20, 25, 50 };
  auto segs = parlay::tabulate( 100 / seg_ratio[seg_mode], [&]( size_t i ) {
    return wi.size() * seg_ratio[seg_mode] / 100 * i;
  } );
  segs.push_back( wi.size() );

  //* batch insert
  if ( tag >= 1 ) {
    if ( ins_mode == 0 )  // unit test
    {
      batchInsert<TreeDesc, point>( pkd, wp, wi, Dim, rounds );
    } else if ( ins_mode == 1 )  // insert all
    {
      const auto& cwi = wi;
      pkd->insert( parlay::make_slice( cwi ) );
    } else if ( ins_mode == 2 )  // insert in seg mode
    {
      for ( size_t i = 1; i < segs.size(); ++i ) {
        const parlay::sequence<point> pwi( wi.begin() + segs[i - 1],
                                           wi.begin() + segs[i] );
        pkd->insert( parlay::make_slice( pwi ) );
      }
    }
    // ins_mode==3 is reserved as no op
    else if ( ins_mode == 4 )  // insert in partial mode
    {
      const auto ins_size = size_t( wi.size() ) * ins_ratio / 10;
      parlay::sequence<point> pwi( wi.begin(), wi.begin() + ins_size );
      batchInsert<TreeDesc, point>( pkd, wp, pwi, Dim, rounds );
    } else if ( ins_mode == 5 )  // insert in partial mode (size varies exponentially)
    {
      auto ins_size = size_t( wi.size() ) * 2;
      for ( int i = 1; i <= ins_ratio; ++i ) {
        ins_size /= 10;
        parlay::sequence<point> pwi( wi.begin(), wi.begin() + ins_size );
        batchInsert<TreeDesc, point>( pkd, wp, pwi, Dim, rounds );
      }
    }

    if ( tag == 1 ) {
      wp.append( wi );
    }
  } else {
    std::cout << "-1 " << std::flush;
  }

  //* batch delete
  if ( tag >= 2 ) {
    assert( wi.size() );
    if ( del_mode == 0 )  // unit test
    {
      batchDelete<TreeDesc, point>( pkd, wp, wi, Dim, rounds );
    } else if ( del_mode == 1 )  // delete all
    {
      auto wi2 = wi;
      pkd->bulk_erase( wi2 );
    } else if ( del_mode == 2 )  // delete in seg mode
    {
      for ( size_t i = 1; i < segs.size(); ++i ) {
        parlay::sequence<point> pwi( wi.begin() + segs[i - 1], wi.begin() + segs[i] );
        pkd->bulk_erase( pwi );
      }
    }
    // del_mode==3 is reserved as no op
    else if ( del_mode == 4 )  // delete in partial mode
    {
      delete pkd;
      const auto& cwp = wp;
      pkd = new Tree( parlay::make_slice( cwp ) );

      const auto del_size = size_t( wp.size() ) * ins_ratio / 10;
      // ### NOTICE: we test deletion from wp instead of wi
      parlay::sequence<point> pwp( wp.begin(), wp.begin() + del_size );
      batchDelete<TreeDesc, point>( pkd, wp, pwp, Dim, rounds, false, true );
    } else if ( del_mode == 5 )  // delete in partial mode (size varies exponentially)
    {
      auto del_size = size_t( wp.size() ) * 2;
      for ( int i = 1; i <= ins_ratio; ++i ) {
        del_size /= 10;
        // ### NOTICE: we test deletion from wp instead of wi
        parlay::sequence<point> pwp( wp.begin(), wp.begin() + del_size );
        batchDelete<TreeDesc, point>( pkd, wp, pwp, Dim, rounds, false, true );
      }
    }

  } else {
    std::cout << "-1 " << std::flush;
  }

  int queryNum = 1000;

  if ( queryType & ( 1 << 0 ) ) {  //* NN
    kdknn = new Typename[wp.size()];
    if ( downsize_k == 0 ) {
      // #### NOTICE: custom query: restrict the query set to the size of 1%
      auto wq = wp;
      wq.resize( wp.size() / 100 );
      queryKNN<TreeDesc, point>( Dim, wq, rounds, pkd, kdknn, K, false );
    } else
      for ( auto cnt_nbh = K; cnt_nbh > 0; cnt_nbh /= downsize_k ) {
        // parlay::sequence<point> wp_crop(wp.begin(), wp.begin()+1000000);
        // queryKNN<TreeDesc,point>(Dim, wp_crop, rounds, pkd, kdknn, cnt_nbh, false );
        queryKNN<TreeDesc, point>( Dim, wp, rounds, pkd, kdknn, cnt_nbh, false );
      }
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 1 ) ) {  //* range count
    // kdknn = new Typename[queryNum];
    // rangeCount<TreeDesc,point>( wp, pkd, rounds, queryNum );
    std::cout << "-1 " << std::flush;
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 2 ) || (queryType&(1<<16))) {  //* range query
    /*
    if ( !( queryType & ( 1 << 1 ) ) ) {  //* run range count to obtain max candidate size
      kdknn = new Typename[queryNum];

      parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
        box queryBox = pkd.get_box( box( wp[i], wp[i] ),
                                    box( wp[( i + wp.size() / 2 ) % wp.size()],
                                         wp[( i + wp.size() / 2 ) % wp.size()] ) );
        kdknn[i] = pkd.range_count( queryBox );
      } );
    }
    */
    //* reduce
    /*
    auto maxReduceSize = parlay::reduce(
        parlay::delayed_tabulate( queryNum, [&]( size_t i ) { return kdknn[i]; } ),
        parlay::maximum<Typename>() );
    points Out( queryNum * maxReduceSize );
    */
    for ( int num_rect : { 10000 } ) {
      std::cout << "[num_rect " << num_rect << "] " << std::flush;
      for ( int type_rect : { 2 } )
        rangeQuery<TreeDesc, point>( wp, pkd, Dim, rounds, num_rect, type_rect );
    }

  } else {
    std::cout << "-1 " << std::flush;
  }
  if ( queryType & ( 1 << 3 ) ) {
    /*
    generate_knn<point>( Dim, wp, rounds, pkd, kdknn, K, false,
                         "/data9/zmen002/knn/GeoLifeNoScale.pbbs.out" );
    */
  }
  if ( queryType & ( 1 << 4 ) ) {  // single-point range query
    for ( int num_rect : { 100 } ) {
      std::cout << "[num_rect " << num_rect << "] " << std::endl;
      for ( int type_rect : { 0, 1, 2 } )
        rangeQueryWithLog<TreeDesc, point>( wp, pkd, rounds, type_rect, num_rect, Dim );
    }
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

template<class TreeDesc, typename point>
void
bench_osm_sliding_window( int Dim, int LEAVE_WRAP, int K, int rounds, const string& osm_prefix,
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

  int window_size = 5; // NOTICE: we fix the query size 5
  using Tree = typename TreeDesc::type;
  Tree* pkd = nullptr;
  //insertOsmByTime<TreeDesc, point>( Dim, node_by_year, rounds, pkd, qs, K );
  auto gen_wp = [&](size_t end_year, size_t backtrace){
    size_t begin_year = end_year>=backtrace? end_year-backtrace: 0;
    points wp;
    for(size_t i=begin_year; i<end_year; ++i)
    {
        const auto &ins = node_by_year[i];
        wp.insert(wp.end(), ins.begin(), ins.end());
    }
    return wp;
  };
  for(size_t i=0; i<node_by_year.size(); ++i)
  {
    batchInsert<TreeDesc, point>( pkd, gen_wp(i,window_size), node_by_year[i], Dim, rounds );
    if(i>=window_size)
      batchDelete<TreeDesc, point>( pkd, gen_wp(i+1,window_size+1), node_by_year[i-window_size], Dim, rounds );
    queryKNN<TreeDesc, point>( Dim, qs, rounds, pkd, nullptr, K, false );
  }
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
  } else if ( !( tag & 0x40000000 ) ) {  //* construct data byself
    K = 100;
    generate_random_points<PointType<coord, 15>>( wp, 1000000, N, 15 );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  if ( ( tag & 0xf ) >= 1 ) {
    if ( _insertFile == NULL ) {
      int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
      if ( Dim != 2 ) id = ( id + 1 ) % 3;  //! MOD graph number used to test
      if ( !id ) id++;
      int pos = std::string( iFile ).rfind( "/" ) + 1;
      insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
    } else {
      insertFile = std::string( _insertFile );
    }
  }

  assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

  auto run_test = [&]<class Wrapper>( Wrapper ) {
    auto run = [&]( auto dim_wrapper ) {
      constexpr const auto D = decltype( dim_wrapper )::value;
      using point_t = PointType<coord, D>;
      using Desc = typename Wrapper::desc<point_t>;

      if ( ( tag & 0x40000000 ) && ( tag & 0xf ) == 1 ) {
        bench_osm_year<Desc, point_t>( Dim, LEAVE_WRAP, K, rounds, insertFile, tag );
        return;
      }
      if ( ( tag & 0x40000000 ) && ( tag & 0xf ) == 2 ) {
        bench_osm_month<Desc, point_t>( Dim, LEAVE_WRAP, K, rounds, insertFile, tag );
        return;
      }
      if ( ( tag & 0x40000000 ) && ( tag & 0xf ) == 3 ) {
        bench_osm_sliding_window<Desc, point_t>( Dim, LEAVE_WRAP, K, rounds, insertFile, tag );
        return;
      }
      auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, D> {
        return PointType<coord, D>( wp[i].coordinate() );
      } );
      decltype( wp )().swap( wp );
      testParallelKDtree<Desc>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag,
                                queryType );
    };

    if ( tag == -1 ) {
      //* serial run
      // todo rewrite test serial code
      // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    } else if ( Dim == 2 ) {
      run( std::integral_constant<int, 2>{} );
    } else if ( Dim == 3 ) {
      run( std::integral_constant<int, 3>{} );
    } else if ( Dim == 5 ) {
      run( std::integral_constant<int, 5>{} );
    } else if ( Dim == 7 ) {
      run( std::integral_constant<int, 7>{} );
    } else if ( Dim == 9 ) {
      run( std::integral_constant<int, 9>{} );
    } else if ( Dim == 10 ) {
      run( std::integral_constant<int, 10>{} );
    } else if ( Dim == 16 ) {
      run( std::integral_constant<int, 16>{} );
    } else fprintf(stderr, "unsupported dim: %d\n", Dim);
  };

  if ( treeType == 0 )
    run_test( wrapper::COTree_t{} );
  else if ( treeType == 1 )
    run_test( wrapper::BHLTree_t{} );
  else if ( treeType == 2 )
    run_test( wrapper::LogTree_t{} );

  return 0;
}
