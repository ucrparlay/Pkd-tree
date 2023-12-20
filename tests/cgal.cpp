#include "common/parse_command_line.h"

#include "testFramework.h"

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Fuzzy_sphere.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

using Typename = coord;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;
typedef CGAL::Fuzzy_sphere<TreeTraits> Fuzzy_circle;
//@ median tree
typedef CGAL::Median_of_rectangle<TreeTraits> Median_of_rectangle;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Median_of_rectangle>
    Neighbor_search_Median;
typedef Neighbor_search_Median::Tree Tree_Median;

//@ midpoint tree
typedef CGAL::Midpoint_of_rectangle<TreeTraits> Midpoint_of_rectangle;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Midpoint_of_rectangle>
    Neighbor_search_Midpoint;
typedef Neighbor_search_Midpoint::Tree Tree_Midpoint;
typedef CGAL::Fuzzy_iso_box<TreeTraits> Fuzzy_iso_box;

template<typename Splitter, typename Tree, typename Neighbor_search, typename point>
void
testCGALParallel( int Dim, int LEAVE_WRAP, parlay::sequence<point>& wp, int N, int K,
                  const int& rounds, const string& insertFile, const int& tag,
                  const int& queryType ) {
  using points = parlay::sequence<point>;
  using pkdTree = ParallelKDtree<point>;
  using box = typename pkdTree::box;

  parlay::internal::timer timer;

  points wi;
  if ( insertFile != "" ) {
    auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
    if ( nd != Dim ) {
      puts( "read inserted points dimension wrong" );
      abort();
    }
  }
  //* otherwise cannot insert heavy duplicated points
  // wp = parlay::unique( parlay::sort( wp ),
  //                      [&]( const point& a, const point& b ) { return a == b;
  //                      } );
  // wi = parlay::unique( parlay::sort( wi ),
  //                      [&]( const point& a, const point& b ) { return a == b;
  //                      } );
  N = wp.size();

  //* cgal
  std::vector<Point_d> _points( N );
  std::vector<Point_d> _points_insert( wi.size() );
  parlay::parallel_for( 0, N, [&]( size_t i ) {
    _points[i] =
        Point_d( Dim, std::begin( wp[i].pnt ), ( std::begin( wp[i].pnt ) + Dim ) );
  } );
  parlay::parallel_for( 0, wi.size(), [&]( size_t i ) {
    _points_insert[i] =
        Point_d( Dim, std::begin( wi[i].pnt ), ( std::begin( wi[i].pnt ) + Dim ) );
  } );

  timer.start();
  Splitter split;
  Tree tree( _points.begin(), _points.end(), split );
  tree.template build<CGAL::Parallel_tag>();
  timer.stop();

  std::cout << timer.total_time() << " " << tree.root()->depth() << " " << std::flush;

  if ( tag >= 1 ) {
    timer.reset();
    timer.start();
    size_t sz = _points_insert.size() * batchInsertRatio;
    tree.insert( _points_insert.begin(), _points_insert.begin() + sz );
    tree.template build<CGAL::Parallel_tag>();
    std::cout << timer.total_time() << " " << std::flush;

    if ( tag == 1 ) wp.append( wi );
  }

  auto cgal_delete = [&]( bool afterInsert = 1, double ratio = 1.0 ) {
    if ( !afterInsert ) {
      tree.clear();
      tree.insert( _points.begin(), _points.end() );
      tree.template build<CGAL::Parallel_tag>();
    }
    timer.reset();
    timer.start();
    if ( afterInsert ) {
      size_t sz = _points_insert.size() * ratio;
      for ( auto it = _points_insert.begin(); it != _points_insert.begin() + sz; it++ ) {
        tree.remove( *it );
      }
    } else {
      assert( tree.size() == wp.size() );
      size_t sz = _points.size() * ratio;
      for ( auto it = _points.begin(); it != _points.begin() + sz; it++ ) {
        tree.remove( *it );
      }
    }
    std::cout << timer.total_time() << " " << std::flush;
    // assert( tree.root()->num_items() == wp.size() );
  };

  if ( tag >= 2 ) {
    cgal_delete( 0, batchInsertRatio );
  }

  //* start test

  Typename* cgknn;
  if ( tag == 1 ) {
    cgknn = new Typename[N + wi.size()];
  } else {
    cgknn = new Typename[N];
  }
  int queryNum = rangeQueryNum;

  if ( queryType & ( 1 << 0 ) ) {  //* KNN query
    auto run_cgal_knn = [&]( int kth ) {
      timer.reset();
      timer.start();
      parlay::sequence<size_t> visNodeNum( N, 0 );
      tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, N ),
                         [&]( const tbb::blocked_range<std::size_t>& r ) {
                           for ( std::size_t s = r.begin(); s != r.end(); ++s ) {
                             // Neighbor search can be instantiated from
                             // several threads at the same time
                             Point_d query( Dim, std::begin( wp[s].pnt ),
                                            std::begin( wp[s].pnt ) + Dim );
                             Neighbor_search search( tree, query, kth );
                             auto it = search.end();
                             it--;
                             cgknn[s] = it->second;
                             visNodeNum[s] =
                                 search.internals_visited() + search.leafs_visited();
                           }
                         } );
      timer.stop();
      std::cout << timer.total_time() << " " << tree.root()->depth() << " "
                << parlay::reduce( visNodeNum ) / wp.size() << " " << std::flush;
    };

    if ( tag == 0 ) {
      int k[3] = { 1, 10, 100 };
      for ( int i = 0; i < 3; i++ ) {
        run_cgal_knn( k[i] );
      }
    } else {
      run_cgal_knn( K );
    }
  }

  if ( queryType & ( 1 << 1 ) ) {  //* batch query
    timer.reset();
    timer.start();
    parlay::sequence<size_t> visNodeNum( batchQuerySize, 0 );

    tbb::parallel_for(
        tbb::blocked_range<std::size_t>( 0, batchQuerySize ),
        [&]( const tbb::blocked_range<std::size_t>& r ) {
          for ( std::size_t s = r.begin(); s != r.end(); ++s ) {
            // Neighbor search can be instantiated from
            // several threads at the same time
            Point_d query( Dim, std::begin( wp[s].pnt ), std::begin( wp[s].pnt ) + Dim );
            Neighbor_search search( tree, query, K );
            auto it = search.end();
            it--;
            cgknn[s] = it->second;
            visNodeNum[s] = search.internals_visited() + search.leafs_visited();
          }
        } );

    timer.stop();
    std::cout << timer.total_time() << " " << tree.root()->depth() << " "
              << parlay::reduce( visNodeNum ) / batchQuerySize << " " << std::flush;
  }

  if ( queryType & ( 1 << 2 ) ) {  //* range count
    int type[3] = { 0, 1, 2 };
    for ( int i = 0; i < 3; i++ ) {
      size_t n = wp.size();
      std::vector<Point_d> _ans( n );
      auto [queryBox, maxSize] = gen_rectangles( queryNum, type[i], wp, Dim );

      timer.reset();
      timer.start();

      parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
        Point_d a( Dim, std::begin( queryBox[i].first.pnt ),
                   std::end( queryBox[i].first.pnt ) ),
            b( Dim, std::begin( queryBox[i].second.pnt ),
               std::end( queryBox[i].second.pnt ) );
        Fuzzy_iso_box fib( a, b, 0.0 );

        size_t cnt = 0;
        counter_iterator<size_t> cnt_iter( cnt );

        auto it = tree.search( cnt_iter, fib );
        cgknn[i] = cnt;
      } );

      timer.stop();
      std::cout << timer.total_time() << " " << std::flush;
    }
  }

  if ( queryType & ( 1 << 3 ) ) {  //* range query
    auto run_cgal_range_query = [&]( int type ) {
      size_t n = wp.size();
      auto [queryBox, maxSize] = gen_rectangles( queryNum, type, wp, Dim );
      using ref_t = std::reference_wrapper<Point_d>;
      // std::vector<ref_t> out_ref( queryNum * maxSize, std::ref( _points[0] ) );
      std::vector<Point_d> _ans( queryNum * maxSize );

      timer.reset();
      timer.start();

      parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
        Point_d a( Dim, std::begin( queryBox[i].first.pnt ),
                   std::end( queryBox[i].first.pnt ) ),
            b( Dim, std::begin( queryBox[i].second.pnt ),
               std::end( queryBox[i].second.pnt ) );
        Fuzzy_iso_box fib( a, b, 0.0 );
        // auto it = tree.search( out_ref.begin() + i * maxSize, fib );
        // cgknn[i] = std::distance( out_ref.begin() + i * maxSize, it );
        auto it = tree.search( _ans.begin() + i * maxSize, fib );
        cgknn[i] = std::distance( _ans.begin() + i * maxSize, it );
      } );

      timer.stop();
      std::cout << timer.total_time() << " " << std::flush;
    };

    if ( tag == 0 ) {
      const int type[3] = { 0, 1, 2 };
      for ( int i = 0; i < 3; i++ ) {
        run_cgal_range_query( type[i] );
      }
    } else {
      run_cgal_range_query( summaryRangeQueryType );
    }
  }

  if ( queryType & ( 1 << 4 ) ) {  //* batch insert with fraction
    double ratios[10] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
    for ( int i = 0; i < 10; i++ ) {
      tree.clear();

      //* build tree
      _points.resize( wp.size() );
      N = wp.size();
      tree.insert( _points.begin(), _points.end() );
      tree.template build<CGAL::Parallel_tag>();

      auto sz = size_t( wi.size() * ratios[i] );
      timer.reset(), timer.start();
      tree.insert( _points_insert.begin(), _points_insert.begin() + sz );
      timer.stop();
      std::cout << timer.total_time() << " " << std::flush;
    }
  }

  if ( queryType & ( 1 << 5 ) ) {  //* batch deletion with fraction
    double ratios[10] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
    for ( int i = 0; i < 10; i++ ) {
      tree.clear();

      //* build tree
      _points.resize( wp.size() );
      N = wp.size();
      tree.insert( _points.begin(), _points.end() );
      tree.template build<CGAL::Parallel_tag>();

      auto sz = size_t( wp.size() * ratios[i] );
      timer.reset(), timer.start();
      for ( size_t i = 0; i < sz; i++ ) {
        tree.remove( _points[i] );
      }
      timer.stop();
      std::cout << timer.total_time() << " " << std::flush;
    }
  }

  if ( queryType & ( 1 << 6 ) ) {  //* incremental construct
    double ratios[4] = { 0.1, 0.2, 0.25, 0.5 };
    for ( int i = 0; i < 4; i++ ) {
      std::cout << "-1 -1 " << std::flush;
    }
  }

  if ( queryType & ( 1 << 7 ) ) {  //* incremental delete
    double ratios[4] = { 0.1, 0.2, 0.25, 0.5 };
    for ( int i = 0; i < 4; i++ ) {
      std::cout << "-1 -1 " << std::flush;
    }
  }

  if ( queryType & ( 1 << 8 ) ) {  //* incremental then knn
    std::cout << "-1 -1 -1 " << std::flush;
    std::cout << "-1 -1 -1 " << std::flush;
  }

  if ( queryType & ( 1 << 9 ) ) {  //* decremental then knn
    std::cout << "-1 -1 -1 " << std::flush;
    std::cout << "-1 -1 -1 " << std::flush;
  }

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

  using point = PointType<coord, 10>;
  using points = parlay::sequence<point>;

  points wp;
  std::string name, insertFile = "";
  int LEAVE_WRAP = 32;

  //* initialize points
  if ( iFile != NULL ) {
    name = std::string( iFile );
    name = name.substr( name.rfind( "/" ) + 1 );
    std::cout << name << " ";
    auto [n, d] = read_points<point>( iFile, wp, K );
    N = n;
    assert( Dim == d );
  } else {  //* construct data byself
    K = 100;
    generate_random_points<point>( wp, 10000, N, Dim );
    assert( wp.size() == N );
    std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
    std::cout << name << " ";
  }

  if ( readInsertFile == 1 ) {
    int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
    id = ( id + 1 ) % 3;  //! MOD graph number used to test
    if ( !id ) id++;
    int pos = std::string( iFile ).rfind( "/" ) + 1;
    insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
  }

  assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

  if ( tag == -1 ) {
    //* serial run
    // todo rewrite test serial code
    // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
  } else if ( Dim == 2 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 2> {
      return PointType<coord, 2>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median,
                     PointType<coord, 2>>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile,
                                           tag, queryType );
  } else if ( Dim == 3 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 3> {
      return PointType<coord, 3>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median,
                     PointType<coord, 3>>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile,
                                           tag, queryType );
  } else if ( Dim == 5 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 5> {
      return PointType<coord, 5>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median,
                     PointType<coord, 5>>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile,
                                           tag, queryType );
  } else if ( Dim == 7 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 7> {
      return PointType<coord, 7>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median,
                     PointType<coord, 7>>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile,
                                           tag, queryType );
  } else if ( Dim == 9 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 9> {
      return PointType<coord, 9>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median,
                     PointType<coord, 9>>( Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile,
                                           tag, queryType );
  } else if ( Dim == 10 ) {
    auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 10> {
      return PointType<coord, 10>( wp[i].pnt.begin() );
    } );
    decltype( wp )().swap( wp );
    testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median,
                     PointType<coord, 10>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                            insertFile, tag, queryType );
  }

  // else if ( tag == -1 )
  //   testCGALSerial<Median_of_rectangle, Tree_Median, Neighbor_search_Median>(
  //       Dim, LEAVE_WRAP, wp, N, K );

  return 0;
}
