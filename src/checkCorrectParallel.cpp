
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
using point = point10D;
using points = parlay::sequence<point>;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Median_of_rectangle<TreeTraits> Median_of_rectangle;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Median_of_rectangle>
    Neighbor_search;
typedef Neighbor_search::Tree Tree;

int Dim, K, tag, rounds;
bool insert;
size_t N;

void runCGAL( points& wp, points& wi, Typename* cgknn ) {
    //* cgal
    std::vector<Point_d> _points( N );
    parlay::parallel_for(
        0, N,
        [&]( size_t i ) {
            _points[i] = Point_d( Dim, std::begin( wp[i].pnt ),
                                  ( std::begin( wp[i].pnt ) + Dim ) );
        },
        1000 );
    Median_of_rectangle median;
    Tree tree( _points.begin(), _points.end(), median );
    tree.build<CGAL::Parallel_tag>();

    if ( tag == 1 ) {
        _points.resize( wi.size() );
        parlay::parallel_for( 0, wi.size(), [&]( size_t j ) {
            _points[j] = Point_d( Dim, std::begin( wi[j].pnt ),
                                  ( std::begin( wi[j].pnt ) + Dim ) );
        } );
        tree.insert( _points.begin(), _points.end() );
        tree.build<CGAL::Parallel_tag>();
        assert( tree.size() == wp.size() + wi.size() );
        wp.append( wi );
        assert( tree.size() == wp.size() );
        puts( "finish insert to cgal" );
    }

    //* cgal query
    LOG << "begin tbb  query" << ENDL << std::flush;
    assert( tree.is_built() );

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

    wp.pop_tail( wi.size() );
    assert( wp.size() == N );
}

void runKDParallel( points& wp, points& wi, Typename* kdknn ) {
    //* kd tree
    puts( "build kd tree" );
    using pkdtree = ParallelKDtree<point>;
    pkdtree pkd;
    size_t n = wp.size();

    buildTree<point>( Dim, wp, rounds, pkd );
    pkdtree::node* KDParallelRoot = pkd.get_root();
    checkTreeSameSequential<pkdtree>( KDParallelRoot, 0, Dim );

    if ( tag == 1 ) {
        batchInsert<point>( pkd, wp, wi, Dim, 2 );
        assert( checkTreesSize<pkdtree>( pkd.get_root() ) == wp.size() + wi.size() );
        wp.append( wi );
        puts( "inserted points to batch" );
    }

    //* query phase

    assert( N >= K );
    LOG << "begin kd query" << ENDL;
    queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K );

    return;
}

int main( int argc, char* argv[] ) {
    commandLine P( argc, argv,
                   "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                   "<parallelTag>] [-p <inFile>] [-r {1,...,5}]" );
    char* iFile = P.getOptionValue( "-p" );
    K = P.getOptionIntValue( "-k", 100 );
    Dim = P.getOptionIntValue( "-d", 3 );
    N = P.getOptionLongValue( "-n", -1 );
    tag = P.getOptionIntValue( "-t", 0 );
    rounds = P.getOptionIntValue( "-r", 3 );

    if ( tag == 0 ) {
        puts( "build and query" );
    } else if ( tag == 1 ) {
        puts( "build, insert and query" );
    }

    int LEAVE_WRAP = 32;
    points wp;

    //* initialize points
    if ( iFile != NULL ) {
        std::string name( iFile );
        name = name.substr( name.rfind( "/" ) + 1 );
        std::cout << name << " ";
        auto [n, d] = read_points<point>( iFile, wp, K );
        N = n;
        assert( Dim == d );
    } else {  //* construct data byself
        K = 100;
        generate_random_points<point>( wp, 100000, N, Dim );
        assert( wp.size() == N );
        std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
        std::cout << name << " ";
    }

    Typename* cgknn;
    Typename* kdknn;
    points wi;

    if ( tag == 0 ) {
        cgknn = new Typename[N];
        kdknn = new Typename[N];
    } else if ( tag == 1 ) {
        generate_random_points<point>( wi, 1000, N / 2, Dim );
        cgknn = new Typename[N + wi.size()];
        kdknn = new Typename[N + wi.size()];
    }

    runCGAL( wp, wi, cgknn );
    runKDParallel( wp, wi, kdknn );

    //* verify
    for ( size_t i = 0; i < N; i++ ) {
        if ( std::abs( cgknn[i] - kdknn[i] ) > 1e-4 ) {
            puts( "" );
            puts( "wrong" );
            std::cout << i << " " << cgknn[i] << " " << kdknn[i] << std::endl;
            return 0;
        }
    }

    puts( "\nok" );
    return 0;
}