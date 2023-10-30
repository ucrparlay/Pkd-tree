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

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

using Typename = coord;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;

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
  if ( tag >= 1 ) {
    auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
    if ( nd != Dim ) {
      puts( "read inserted points dimension wrong" );
      abort();
    }
  }

  //* otherwise cannot insert heavy duplicated points
  wp = parlay::unique( parlay::sort( wp ),
                       [&]( const point& a, const point& b ) { return a == b; } );
  wi = parlay::unique( parlay::sort( wi ),
                       [&]( const point& a, const point& b ) { return a == b; } );
  N = wp.size();

  //* cgal
  std::vector<Point_d> _points( N );
  parlay::parallel_for(
      0, N,
      [&]( size_t i ) {
        _points[i] =
            Point_d( Dim, std::begin( wp[i].pnt ), ( std::begin( wp[i].pnt ) + Dim ) );
      },
      1000 );

  timer.start();
  Splitter split;
  Tree tree( _points.begin(), _points.end(), split );
  tree.template build<CGAL::Parallel_tag>();
  timer.stop();

  std::cout << timer.total_time() << " " << std::flush;

  if ( tag >= 1 ) {
    _points.resize( wi.size() );
    parlay::parallel_for( 0, wi.size(), [&]( size_t j ) {
      _points[j] =
          Point_d( Dim, std::begin( wi[j].pnt ), ( std::begin( wi[j].pnt ) + Dim ) );
    } );

    timer.reset();
    timer.start();
    tree.insert( _points.begin(), _points.end() );
    tree.template build<CGAL::Parallel_tag>();
    std::cout << timer.total_time() << " " << std::flush;

    if ( tag == 1 ) wp.append( wi );
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( tag >= 2 ) {
    assert( _points.size() == wi.size() );

    timer.reset();
    timer.start();
    for ( auto p : _points ) {
      tree.remove( p );
    }
    std::cout << timer.total_time() << " " << std::flush;
    assert( tree.root()->num_items() == wp.size() );
  } else {
    std::cout << "-1 " << std::flush;
  }

  //* start test

  Typename* cgknn;
  if ( tag == 1 ) {
    cgknn = new Typename[N + wi.size()];
  } else {
    cgknn = new Typename[N];
  }
  size_t maxReduceSize = 0;
  size_t queryNum = 1000;

  if ( queryType & ( 1 << 0 ) ) {  //* NN query
    timer.reset();
    timer.start();

    tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, N ),
                       [&]( const tbb::blocked_range<std::size_t>& r ) {
                         for ( std::size_t s = r.begin(); s != r.end(); ++s ) {
                           // Neighbor search can be instantiated from
                           // several threads at the same time
                           Point_d query( Dim, std::begin( wp[s].pnt ),
                                          std::begin( wp[s].pnt ) + Dim );
                           Neighbor_search search( tree, query, K );
                           auto it = search.end();
                           it--;
                           cgknn[s] = it->second;
                         }
                       } );

    timer.stop();
    std::cout << timer.total_time() << " -1 -1 " << std::flush;
  }

  if ( queryType & ( 1 << 1 ) ) {  //* range count
    size_t n = wp.size();
    std::vector<Point_d> _ans( n );

    timer.reset();
    timer.start();
    for ( size_t i = 0; i < queryNum; i++ ) {
      Point_d a( Dim, std::begin( wp[i].pnt ), std::end( wp[i].pnt ) ),
          b( Dim, std::begin( wp[( i + n / 2 ) % n].pnt ),
             std::end( wp[( i + n / 2 ) % n].pnt ) );
      Fuzzy_iso_box fib( a, b, 0.0 );
      auto it = tree.search( _ans.begin(), fib );
      cgknn[i] = std::distance( _ans.begin(), it );
    }
    timer.stop();
    std::cout << timer.total_time() << " " << std::flush;
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 2 ) ) {  //* range query
    size_t n = wp.size();
    std::vector<Point_d> _ans( n );

    timer.reset();
    timer.start();
    for ( size_t i = 0; i < queryNum; i++ ) {
      Point_d a( Dim, std::begin( wp[i].pnt ), std::end( wp[i].pnt ) ),
          b( Dim, std::begin( wp[( i + n / 2 ) % n].pnt ),
             std::end( wp[( i + n / 2 ) % n].pnt ) );
      Fuzzy_iso_box fib( a, b, 0.0 );
      auto it = tree.search( _ans.begin(), fib );
      cgknn[i] = std::distance( _ans.begin(), it );
    }
    timer.stop();
    std::cout << timer.total_time() << " " << std::flush;
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 4 ) ) {  //* vary k for knn
    int k[3] = { 1, 10, 100 };
    for ( int i = 0; i < 3; i++ ) {
      timer.reset();
      timer.start();

      tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, N ),
                         [&]( const tbb::blocked_range<std::size_t>& r ) {
                           for ( std::size_t s = r.begin(); s != r.end(); ++s ) {
                             // Neighbor search can be instantiated from
                             // several threads at the same time
                             Point_d query( Dim, std::begin( wp[s].pnt ),
                                            std::begin( wp[s].pnt ) + Dim );
                             Neighbor_search search( tree, query, k[i] );
                             auto it = search.end();
                             it--;
                             cgknn[s] = it->second;
                           }
                         } );

      timer.stop();
      std::cout << timer.total_time() << " -1 -1 " << std::flush;
    }
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 5 ) ) {  //* vary box size
    double ratios[3] = { 0.05, 0.2, 0.5 };
    size_t n = wp.size();
    std::vector<Point_d> _ans( n );

    for ( int i = 0; i < 3; i++ ) {
      //* new box
      box queryBox = pkdTree::get_box( parlay::make_slice( wp ) );
      // LOG << queryBox.first << " " << queryBox.second << ENDL;
      auto dim = queryBox.first.get_dim();
      for ( int j = 0; j < dim; j++ ) {
        coord diff = queryBox.second.pnt[j] - queryBox.first.pnt[j];
        queryBox.second.pnt[j] = queryBox.first.pnt[j] + coord( diff * ratios[i] );
      }

      timer.reset();
      timer.start();

      Point_d a( Dim, std::begin( queryBox.first.pnt ), std::end( queryBox.first.pnt ) );
      Point_d b( Dim, std::begin( queryBox.second.pnt ),
                 std::end( queryBox.second.pnt ) );
      Fuzzy_iso_box fib( a, b, 0.0 );
      auto it = tree.search( _ans.begin(), fib );
      cgknn[i] = std::distance( _ans.begin(), it );
      // std::cout << cgknn[i] << queryBox.first << " " << queryBox.second << std::endl;
      timer.stop();
      std::cout << timer.total_time() << " " << cgknn[i] << " " << std::flush;
    }
  } else {
    std::cout << "-1 " << std::flush;
  }

  if ( queryType & ( 1 << 6 ) ) {  //* batch knn
    tree.clear();

    //* build tree
    _points.resize( wp.size() );
    N = wp.size();
    parlay::parallel_for(
        0, N,
        [&]( size_t i ) {
          _points[i] =
              Point_d( Dim, std::begin( wp[i].pnt ), ( std::begin( wp[i].pnt ) + Dim ) );
        },
        1000 );
    tree.insert( _points.begin(), _points.end() );
    tree.template build<CGAL::Parallel_tag>();

    //* initialize insert points
    _points.resize( wi.size() );
    parlay::parallel_for( 0, wi.size(), [&]( size_t j ) {
      _points[j] =
          Point_d( Dim, std::begin( wi[j].pnt ), ( std::begin( wi[j].pnt ) + Dim ) );
    } );

    delete[] cgknn;
    cgknn = new Typename[N + wi.size()];

    double ratios[4] = { 0.1, 0.3, 0.5, 1.0 };
    for ( int i = 0; i < 4; i++ ) {
      auto sz = size_t( wi.size() * ratios[i] );
      tree.insert( _points.begin(), _points.begin() + sz );
      wp.append( wi.begin(), wi.begin() + sz );

      timer.reset();
      timer.start();
      tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, N ),
                         [&]( const tbb::blocked_range<std::size_t>& r ) {
                           for ( std::size_t s = r.begin(); s != r.end(); ++s ) {
                             // Neighbor search can be instantiated from
                             // several threads at the same time
                             Point_d query( Dim, std::begin( wp[s].pnt ),
                                            std::begin( wp[s].pnt ) + Dim );
                             Neighbor_search search( tree, query, K );
                             auto it = search.end();
                             it--;
                             cgknn[s] = it->second;
                           }
                         } );

      timer.stop();
      std::cout << timer.total_time() << " -1 -1 " << std::flush;

      //* restore state
      wp.pop_tail( sz );
      for ( int i = 0; i < sz; i++ ) {
        tree.remove( _points[i] );
      }
    }

  } else {
    std::cout << "-1 " << std::flush;
  }

  std::cout << std::endl << std::flush;

  return;
}

int
main( int argc, char* argv[] ) {
  commandLine P(
      argc, argv,
      "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
      "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i <_insertFile>]" );
  char* iFile = P.getOptionValue( "-p" );
  int K = P.getOptionIntValue( "-k", 100 );
  int Dim = P.getOptionIntValue( "-d", 3 );
  size_t N = P.getOptionLongValue( "-n", -1 );
  int tag = P.getOptionIntValue( "-t", 0 );
  int rounds = P.getOptionIntValue( "-r", 3 );
  int queryType = P.getOptionIntValue( "-q", 0 );
  char* _insertFile = P.getOptionValue( "-i" );

  using point = PointType<coord, 10>;
  using points = parlay::sequence<point>;

  points wp;
  std::string name, insertFile;
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
  }

  // else if ( tag == -1 )
  //   testCGALSerial<Median_of_rectangle, Tree_Median, Neighbor_search_Median>(
  //       Dim, LEAVE_WRAP, wp, N, K );

  return 0;
}