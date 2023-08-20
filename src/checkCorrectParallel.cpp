
#include "testFramework.h"

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <CGAL/Fuzzy_iso_box.h>
#include <bits/stdc++.h>
#include <iterator>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
using point = PointType<coord, 5>;
using points = parlay::sequence<point>;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Random_points_in_cube_d<Point_d> Random_points_iterator;
typedef CGAL::Counting_iterator<Random_points_iterator> N_Random_points_iterator;
typedef CGAL::Median_of_rectangle<TreeTraits> Median_of_rectangle;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Median_of_rectangle>
    Neighbor_search;
typedef Neighbor_search::Tree Tree;
typedef CGAL::Fuzzy_iso_box<TreeTraits> Fuzzy_iso_box;

int Dim, K, tag, rounds;
bool insert;
int queryType;
size_t N;

void
runCGAL( points& wp, points& wi, Typename* cgknn, int queryNum ) {
  //* cgal
  std::vector<Point_d> _points( N );
  parlay::parallel_for(
      0, N,
      [&]( size_t i ) {
        _points[i] =
            Point_d( Dim, std::begin( wp[i].pnt ), ( std::begin( wp[i].pnt ) + Dim ) );
      },
      1000 );
  Median_of_rectangle median;
  Tree tree( _points.begin(), _points.end(), median );
  tree.build<CGAL::Parallel_tag>();

  // LOG << tree.bounding_box() << ENDL;

  if ( tag >= 1 ) {
    _points.resize( wi.size() );
    parlay::parallel_for( 0, wi.size(), [&]( size_t j ) {
      _points[j] =
          Point_d( Dim, std::begin( wi[j].pnt ), ( std::begin( wi[j].pnt ) + Dim ) );
    } );
    tree.insert( _points.begin(), _points.end() );
    tree.build<CGAL::Parallel_tag>();
    assert( tree.size() == wp.size() + wi.size() );
    wp.append( wi );
    assert( tree.size() == wp.size() );
    puts( "finish insert to cgal" );
  }

  if ( tag >= 2 ) {
    assert( _points.size() == wi.size() );
    for ( auto p : _points ) {
      tree.remove( p );
    }
    assert( tree.size() == wp.size() );
    wp.pop_tail( wi.size() );
    puts( "finish delete from cgal" );
  }

  //* cgal query
  LOG << "begin tbb query" << ENDL << std::flush;
  assert( tree.is_built() );

  if ( queryType == 0 ) {  //* NN
    tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, wp.size() ),
                       [&]( const tbb::blocked_range<std::size_t>& r ) {
                         for ( std::size_t s = r.begin(); s != r.end(); ++s ) {
                           // Neighbor search can be instantiated from
                           // several threads at the same time
                           Point_d query( Dim, std::begin( wp[s].pnt ),
                                          std::begin( wp[s].pnt ) + Dim );
                           Neighbor_search search( tree, query, K );
                           Neighbor_search::iterator it = search.end();
                           it--;
                           cgknn[s] = it->second;
                         }
                       } );
  } else if ( queryType == 1 ) {  //* range count
    size_t n = wp.size();
    parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
      std::vector<Point_d> _ans( n );
      Point_d a( Dim, std::begin( wp[i].pnt ), std::end( wp[i].pnt ) ),
          b( Dim, std::begin( wp[( i + n / 2 ) % n].pnt ),
             std::end( wp[( i + n / 2 ) % n].pnt ) );
      Fuzzy_iso_box fib( a, b, 0.0 );
      // _ans.clear();
      auto it = tree.search( _ans.begin(), fib );
      cgknn[i] = std::distance( _ans.begin(), it );
    } );
  }

  if ( tag == 1 ) {
    wp.pop_tail( wi.size() );
    assert( wp.size() == N );
  }
  tree.clear();
}

void
runKDParallel( points& wp, const points& wi, Typename* kdknn, int queryNum ) {
  //* kd tree
  puts( "build kd tree" );
  using pkdtree = ParallelKDtree<point>;
  using box = typename ParallelKDtree<point>::box;
  pkdtree pkd;
  size_t n = wp.size();

  buildTree<point>( Dim, wp, rounds, pkd );
  pkdtree::node* KDParallelRoot = pkd.get_root();
  pkd.validate( Dim );

  if ( tag >= 1 ) {
    batchInsert<point>( pkd, wp, wi, Dim, 2 );
    if ( tag == 1 ) wp.append( wi );
    pkd.validate( Dim );
    LOG << "finish insert" << ENDL;
  }

  if ( tag >= 2 ) {
    batchDelete<point>( pkd, wp, wi, Dim, 2 );
    pkd.validate( Dim );
    LOG << "finish delete" << ENDL;
  }

  //* query phase

  assert( N >= K );
  assert( tag == 1 || wp.size() == N );
  LOG << "begin kd query" << ENDL;
  if ( queryType == 0 ) {
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K );
  } else if ( queryType == 1 ) {
    rangeCount<point>( wp, pkd, kdknn, rounds, queryNum );
  }
  pkd.delete_tree();
  return;
}

int
main( int argc, char* argv[] ) {
  commandLine P(
      argc, argv,
      "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
      "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i <_insertFile>]" );
  char* iFile = P.getOptionValue( "-p" );
  K = P.getOptionIntValue( "-k", 100 );
  Dim = P.getOptionIntValue( "-d", 3 );
  N = P.getOptionLongValue( "-n", -1 );
  tag = P.getOptionIntValue( "-t", 0 );
  rounds = P.getOptionIntValue( "-r", 3 );
  queryType = P.getOptionIntValue( "-q", 0 );
  char* _insertFile = P.getOptionValue( "-i" );

  assert( Dim == point().get_dim() );

  if ( tag == 0 ) {
    puts( "build and query" );
  } else if ( tag == 1 ) {
    puts( "build, insert and query" );
  }

  int LEAVE_WRAP = 32;
  points wp;
  std::string name, insertFile;

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

  Typename* cgknn;
  Typename* kdknn;
  points wi;
  int queryNum = 1000;

  //* initialize insert points file
  if ( tag >= 1 && iFile != NULL ) {
    if ( _insertFile == NULL ) {
      int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
      id = ( id + 1 ) % 10;  //! MOD graph number used to test
      if ( !id ) id++;
      int pos = std::string( iFile ).rfind( "/" ) + 1;
      insertFile = std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
    } else {
      insertFile = std::string( _insertFile );
    }
    std::cout << insertFile << ENDL;
  }

  //* set result array size
  if ( queryType == 0 ) {  //*NN
    LOG << "---do NN query---" << ENDL;
    if ( tag == 0 ) {
      cgknn = new Typename[N];
      kdknn = new Typename[N];
    } else {
      if ( iFile == NULL ) {
        generate_random_points<point>( wi, 1000000, N / 2, Dim );
        LOG << "insert " << N / 5 << " points" << ENDL;
      } else {
        auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
        if ( nd != Dim || nn != N ) {
          puts( "read inserted points dimension wrong" );
          abort();
        } else {
          puts( "read inserted points from file" );
        }
      }

      if ( tag == 1 ) {
        puts( "insert points from file" );
        cgknn = new Typename[N + wi.size()];
        kdknn = new Typename[N + wi.size()];
      } else if ( tag == 2 ) {
        puts( "insert then delete points from file" );
        cgknn = new Typename[N];
        kdknn = new Typename[N];
      }
    }
  } else if ( queryType == 1 ) {  //* range Count
    LOG << "---do range Count---" << ENDL;

    cgknn = new Typename[queryNum];
    kdknn = new Typename[queryNum];
  } else if ( queryType == 2 ) {
  }

  runCGAL( wp, wi, cgknn, queryNum );

  runKDParallel( wp, wi, kdknn, queryNum );

  //* verify
  if ( queryType == 0 ) {
    for ( size_t i = 0; i < N; i++ ) {
      if ( std::abs( cgknn[i] - kdknn[i] ) > 1e-4 ) {
        puts( "" );
        puts( "wrong" );
        std::cout << i << " " << cgknn[i] << " " << kdknn[i] << std::endl;
        return 0;
      }
    }
  } else if ( queryType == 1 ) {
    for ( size_t i = 0; i < queryNum; i++ ) {
      if ( std::abs( cgknn[i] - kdknn[i] ) > 1e-4 ) {
        puts( "" );
        puts( "wrong" );
        std::cout << i << " " << cgknn[i] << " " << kdknn[i] << std::endl;
        return 0;
      }
    }
  }

  puts( "\nok" );
  return 0;
}