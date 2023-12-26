#include <algorithm>
#include <cstdlib>
#include "testFramework.h"

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
  using boxs = parlay::sequence<box>;

  // auto boxs = gen_rectangles<point>( 1000, 2, wp, Dim );
  // for ( int i = 0; i < 10; i++ ) {
  //   LOG << boxs[i].first << " " << boxs[i].second << ENDL;
  // }
  // return;

  if ( N != wp.size() ) {
    puts( "input parameter N is different to input points size" );
    abort();
  }

  tree pkd;

  points wi;
  if ( insertFile != "" ) {
    auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
    if ( nd != Dim ) {
      puts( "read inserted points dimension wrong" );
      abort();
    }
  }

  Typename* kdknn = nullptr;

  //* begin test
  buildTree<point>( Dim, wp, rounds, pkd );

  //* batch insert
  if ( tag >= 1 ) {
    batchInsert<point>( pkd, wp, wi, Dim, rounds, batchInsertRatio );
  }

  //* batch delete
  if ( tag >= 2 ) {
    batchDelete<point>( pkd, wp, wi, Dim, rounds, 0, batchInsertRatio );
  }

  if ( queryType & ( 1 << 0 ) ) {  //* KNN
    kdknn = new Typename[wp.size()];
    if ( tag == 0 ) {
      int k[3] = { 1, 10, 100 };
      for ( int i = 0; i < 3; i++ ) {
        queryKNN<point>( Dim, wp, rounds, pkd, kdknn, k[i], false );
      }
    } else {  // test summary
      queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );
    }
    delete[] kdknn;
  }

  if ( queryType & ( 1 << 1 ) ) {  //* batch NN query
    points new_wp( batchQuerySize );
    parlay::copy( wp.cut( 0, batchQuerySize ), new_wp.cut( 0, batchQuerySize ) );
    kdknn = new Typename[batchQuerySize];

    queryKNN( Dim, new_wp, rounds, pkd, kdknn, K, true );

    delete[] kdknn;
  }

  int recNum = rangeQueryNum;

  if ( queryType & ( 1 << 2 ) ) {  //* range count
    kdknn = new Typename[recNum];
    int type[3] = { 0, 1, 2 };

    for ( int i = 0; i < 3; i++ ) {
      rangeCountFix<point>( wp, pkd, kdknn, rounds, type[i], recNum, Dim );
    }

    delete[] kdknn;
  }

  if ( queryType & ( 1 << 3 ) ) {  //* range query

    if ( tag == 0 ) {
      const int type[3] = { 0, 1, 2 };
      for ( int i = 0; i < 3; i++ ) {
        //* run range count to obtain size
        kdknn = new Typename[recNum];
        points Out;
        //* range query
        rangeQueryFix<point>( wp, pkd, kdknn, rounds, Out, type[i], recNum, Dim );
      }
    } else if ( tag == 2 ) {
      kdknn = new Typename[recNum];
      points Out;
      rangeQueryFix<point>( wp, pkd, kdknn, rounds, Out, summaryRangeQueryType, recNum,
                            Dim );
    }

    delete[] kdknn;
  }

  // if ( queryType & ( 1 << 3 ) ) {  //* generate knn
  // generate_knn<point>( Dim, wp, K,
  // "/data9/zmen002/knn/GeoLifeNoScale.pbbs.out"
  // );
  // }

  if ( queryType & ( 1 << 4 ) ) {  //* batch insertion with fraction
    double ratios[10] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
    for ( int i = 0; i < 10; i++ ) {
      batchInsert<point>( pkd, wp, wi, Dim, rounds, ratios[i] );
    }
  }

  if ( queryType & ( 1 << 5 ) ) {  //* batch deletion with fraction
    const double ratios[10] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
    // double ratios[10] = { 1.0 };
    points tmp;
    for ( int i = 0; i < 10; i++ ) {
      batchDelete<point>( pkd, wp, tmp, Dim, rounds, 0, ratios[i] );
    }
  }

  if ( queryType & ( 1 << 6 ) ) {  //* incremental Build
    double step[4] = { 0.1, 0.2, 0.25, 0.5 };
    for ( int i = 0; i < 4; i++ ) {
      incrementalBuild<point>( Dim, wp, rounds, pkd, step[i] );
    }
  }

  if ( queryType & ( 1 << 7 ) ) {  //* incremental Delete
    double step[4] = { 0.1, 0.2, 0.25, 0.5 };
    for ( int i = 0; i < 4; i++ ) {
      incrementalDelete<point>( Dim, wp, wi, rounds, pkd, step[i] );
    }
  }

  if ( queryType & ( 1 << 8 ) ) {  //* batch insertion then knn
    kdknn = new Typename[wp.size()];

    //* first normal build
    buildTree<point, 0>( Dim, wp, rounds, pkd );
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

    //* then incremental build
    incrementalBuild<point, 0>( Dim, wp, rounds, pkd, 0.1 );
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

    delete[] kdknn;
  }

  if ( queryType & ( 1 << 9 ) ) {  //* batch deletion then knn
    kdknn = new Typename[wp.size()];

    //* first normal build
    buildTree<point, 0>( Dim, wp, rounds, pkd );
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

    //* then incremental delete
    incrementalDelete<point, 0>( Dim, wp, wi, rounds, pkd, 0.1 );
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

    delete[] kdknn;
  }

  if ( queryType & ( 1 << 10 ) ) {  //* test inbalance ratio
    points np, nq;
    std::string prefix, path;
    size_t batchSize = wp.size() / 10;
    kdknn = new Typename[wp.size()];

    auto inbaQueryType = std::stoi( std::getenv( "INBA_QUERY" ) );
    auto inbaBuildType = std::stoi( std::getenv( "INBA_BUILD" ) );

    //@ helper functions
    auto clean = [&]() {
      prefix = insertFile.substr( 0, insertFile.rfind( "/" ) );
      np.clear();
      nq.clear();
    };

    auto run = [&]() {
      if ( inbaBuildType == 0 ) {
        buildTree<point, 2>( Dim, np, rounds, pkd );
      } else {
        incrementalBuild<point, 2>( Dim, np, rounds, pkd, 0.01 );
      }

      if ( inbaQueryType == 0 ) {
        const int k[3] = { 1, 5, 100 };
        for ( int i = 0; i < 3; i++ ) {
          queryKNN<point, 0, 1>( Dim, np, rounds, pkd, kdknn, k[i], false );
        }
      } else if ( inbaQueryType == 1 ) {
        int type = 2;
        rangeCountFix<point>( wp, pkd, kdknn, rounds, type, rangeQueryNumInbaRatio, Dim );
      }
    };

    LOG << "alpha: " << pkd.get_imbalance_ratio() << ENDL;
    //! start with varden file
    //@ 1: 10*0.1 different vardens.
    clean();
    for ( int i = 1; i <= 10; i++ ) {
      path = prefix + "/" + std::to_string( i ) + ".in";
      // std::cout << path << std::endl;
      read_points<point>( path.c_str(), nq, K );
      np.append( nq.cut( 0, batchSize ) );
      nq.clear();
    }
    assert( np.size() == wp.size() );
    run();

    //@ 2: 1 uniform, and 9*0.1 same varden
    //* read varden first
    clean();
    path = prefix + "/1.in";
    // std::cout << path << std::endl;
    read_points<point>( path.c_str(), np, K );
    //* then read uniforprefixm
    prefix = prefix.substr( 0, prefix.rfind( "/" ) );  // 1000000_3
    prefix = prefix.substr( 0, prefix.rfind( "/" ) );  // ss_varden
    path = prefix + "/uniform/" + std::to_string( np.size() ) + "_" +
           std::to_string( Dim ) + "/1.in";
    // std::cout << path << std::endl;

    read_points<point>( path.c_str(), nq, K );
    parlay::parallel_for( 0, batchSize, [&]( size_t i ) { np[i] = nq[i]; } );
    run();

    //@ 3: 1 varden, but flatten;
    clean();
    path = prefix + "/1.in";
    // std::cout << path << std::endl;
    read_points<point>( path.c_str(), np, K );
    buildTree<point, 0>( Dim, np, rounds, pkd );
    pkd.flatten( pkd.get_root(), parlay::make_slice( np ) );
    run();

    delete[] kdknn;
  }

  // generate_knn( Dim, wp, K, "knn.out" );

  std::cout << std::endl << std::flush;

  return;
}

int
main( int argc, char* argv[] ) {
  commandLine P( argc, argv,
                 "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                 "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i "
                 "<_insertFile>]" );
  char* iFile = P.getOptionValue( "-p" );
  int K = P.getOptionIntValue( "-k", 100 );
  int Dim = P.getOptionIntValue( "-d", 3 );
  size_t N = P.getOptionLongValue( "-n", -1 );
  int tag = P.getOptionIntValue( "-t", 1 );
  int rounds = P.getOptionIntValue( "-r", 3 );
  int queryType = P.getOptionIntValue( "-q", 0 );
  int readInsertFile = P.getOptionIntValue( "-i", 1 );

  int LEAVE_WRAP = 32;
  parlay::sequence<PointType<coord, 15>> wp;
  // parlay::sequence<PointID<coord, 15>> wp;
  std::string name, insertFile = "";

  //* initialize points
  if ( iFile != NULL ) {
    name = std::string( iFile );
    name = name.substr( name.rfind( "/" ) + 1 );
    std::cout << name << " ";
    auto [n, d] = read_points<PointType<coord, 15>>( iFile, wp, K );
    // auto [n, d] = read_points<PointID<coord, 15>>( iFile, wp, K );
    N = n;
    assert( d == Dim );
  } else {  //* construct data byself
    K = 100;
    generate_random_points<PointType<coord, 15>>( wp, 1000000, N, Dim );
    // generate_random_points<PointID<coord, 15>>( wp, 1000000, N, Dim );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  if ( readInsertFile == 1 ) {
    int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
    if ( Dim != 2 ) id = ( id + 1 ) % 3;  //! MOD graph number used to test
    if ( !id ) id++;
    int pos = std::string( iFile ).rfind( "/" ) + 1;
    insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
  }

  assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

  // if ( tag == -1 ) {
  //   //* serial run
  //   // todo rewrite test serial code
  //   // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
  // } else if ( Dim == 2 ) {
  //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 2> {
  //     return PointID<coord, 2>( wp[i].pnt.begin(), i );
  //   } );
  //   decltype( wp )().swap( wp );
  //   testParallelKDtree<PointID<coord, 2>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
  //   insertFile,
  //                                          tag, queryType );
  // } else if ( Dim == 3 ) {
  //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 3> {
  //     return PointID<coord, 3>( wp[i].pnt.begin(), i );
  //   } );
  //   decltype( wp )().swap( wp );
  //   testParallelKDtree<PointID<coord, 3>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
  //   insertFile,
  //                                          tag, queryType );
  // } else if ( Dim == 5 ) {
  //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 5> {
  //     return PointID<coord, 5>( wp[i].pnt.begin(), i );
  //   } );
  //   decltype( wp )().swap( wp );
  //   testParallelKDtree<PointID<coord, 5>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
  //   insertFile,
  //                                          tag, queryType );
  // } else if ( Dim == 7 ) {
  //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 7> {
  //     return PointID<coord, 7>( wp[i].pnt.begin(), i );
  //   } );
  //   decltype( wp )().swap( wp );
  //   testParallelKDtree<PointID<coord, 7>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
  //   insertFile,
  //                                          tag, queryType );
  // }
  //
  if ( tag == -1 ) {
    //* serial run
    // todo rewrite test serial code
    // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
  } else if ( Dim == 2 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 2> {
      return PointType<coord, 2>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<coord, 2>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                             insertFile, tag, queryType );
  } else if ( Dim == 3 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 3> {
      return PointType<coord, 3>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<coord, 3>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                             insertFile, tag, queryType );
  } else if ( Dim == 5 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 5> {
      return PointType<coord, 5>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<coord, 5>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                             insertFile, tag, queryType );
  } else if ( Dim == 7 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 7> {
      return PointType<coord, 7>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<coord, 7>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                             insertFile, tag, queryType );
  } else if ( Dim == 9 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 9> {
      return PointType<coord, 9>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<coord, 9>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                             insertFile, tag, queryType );
  } else if ( Dim == 10 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 10> {
      return PointType<coord, 10>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testParallelKDtree<PointType<coord, 10>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                              insertFile, tag, queryType );
  }

  return 0;
}
