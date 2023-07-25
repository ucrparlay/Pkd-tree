
#include "testFramework.h"

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <bits/stdc++.h>
#include <iterator>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
using points = parlay::sequence<point10D>;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Median_of_rectangle<TreeTraits> Median_of_rectangle;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Median_of_rectangle>
    Neighbor_search;
typedef Neighbor_search::Tree Tree;

int Dim, Q, K;
long N;

void runCGAL( const points& wp, Typename* cgknn ) {
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

  //* cgal query
  LOG << "begin tbb  query" << ENDL << std::flush;

  tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, _points.size() ),
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
}

void runKDParallel( const points& wp, Typename* kdknn ) {
  //* kd tree
  puts( "build kd tree" );
  using pkdtree = ParallelKDtree<point10D>;
  pkdtree pkd;
  points wo, wx;
  size_t n = wp.size();
  int rounds = 3;

  buildTree( Dim, wp, rounds, pkd );
  pkdtree::node* KDParallelRoot = pkd.get_root();
  checkTreeSameSequential<pkdtree>( KDParallelRoot, 0, Dim );

  //* query phase

  assert( N >= K );
  LOG << "begin kd query" << ENDL;
  queryKNN( Dim, wp, rounds, pkd, kdknn, K );

  return;
}

int main( int argc, char* argv[] ) {
  std::cout.precision( 5 );
  points wp;
  std::string name( argv[1] );

  if ( name.find( "/" ) != std::string::npos ) {
    name = name.substr( name.rfind( "/" ) + 1 );
    std::cout << name << " ";

    freopen( argv[1], "r", stdin );
    K = std::stoi( argv[2] );

    scanf( "%ld%d", &N, &Dim );
    wp.resize( N );
    for ( int i = 0; i < N; i++ ) {
      for ( int j = 0; j < Dim; j++ ) {
        scanf( "%ld", &wp[i].pnt[j] );
      }
    }
  } else {
    K = 100;
    size_t box_size = 1000000;

    std::random_device rd;        // a seed source for the random number engine
    std::mt19937 gen_mt( rd() );  // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int> distrib( 1, box_size );

    parlay::random_generator gen( distrib( gen_mt ) );
    std::uniform_int_distribution<int> dis( 0, box_size );

    assert( argc >= 3 );
    size_t n = std::stoi( argv[1] );
    N = n;
    Dim = std::stoi( argv[2] );
    wp.resize( N );
    //* generate n random points in a cube
    parlay::parallel_for( 0, n, [&]( long i ) {
      auto r = gen[i];
      for ( int j = 0; j < Dim; j++ ) {
        wp[i].pnt[j] = dis( r );
      }
    } );
  }

  Typename* cgknn = new Typename[N];
  Typename* kdknn = new Typename[N];

  runCGAL( wp, cgknn );
  runKDParallel( wp, kdknn );

  //* verify
  for ( size_t i = 0; i < N; i++ ) {
    if ( std::abs( cgknn[i] - kdknn[i] ) > 1e-4 ) {
      puts( "" );
      puts( "wrong" );
      std::cout << i << " " << cgknn[i] << " " << kdknn[i] << std::endl;
      return 0;
    }
  }

  puts( "ok" );
  return 0;
}