#pragma once

#include "kdTree.h"
#include "kdTreeParallel.h"

#include "common/geometryIO.h"
#include "common/parse_command_line.h"
#include "common/time_loop.h"

using Typename = coord;

double aveDeep = 0.0;

//*---------- generate points within a 0-box_size --------------------
template<typename point>
void generate_random_points( parlay::sequence<point>& wp, coord _box_size, long n,
                             int Dim ) {
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
std::pair<size_t, int> read_points( const char* iFile, parlay::sequence<point>& wp,
                                    int K ) {
    parlay::sequence<char> S = readStringFromFile( iFile );
    parlay::sequence<char*> W = stringToWords( S );
    size_t N = atol( W[0] );
    int Dim = atoi( W[1] );
    assert( N >= 0 && Dim >= 1 && N >= K );

    auto pts = W.cut( 2, W.size() );
    assert( pts.size() % Dim == 0 );
    size_t n = pts.size() / Dim;
    auto a =
        parlay::tabulate( Dim * n, [&]( size_t i ) -> coord { return atol( pts[i] ); } );
    wp.resize( N );
    parlay::parallel_for( 0, n, [&]( size_t i ) {
        for ( int j = 0; j < Dim; j++ ) {
            wp[i].pnt[j] = a[i * Dim + j];
        }
    } );
    return std::make_pair( N, Dim );
}

void traverseSerialTree( KDnode<Typename>* KDroot, int deep ) {
    if ( KDroot->isLeaf ) {
        aveDeep += deep;
        return;
    }
    traverseSerialTree( KDroot->left, deep + 1 );
    traverseSerialTree( KDroot->right, deep + 1 );
    return;
}

template<typename tree>
void traverseParallelTree( typename tree::node* root, int deep ) {
    if ( root->is_leaf ) {
        aveDeep += deep;
        return;
    }
    typename tree::interior* TI = static_cast<typename tree::interior*>( root );
    traverseParallelTree<tree>( TI->left, deep + 1 );
    traverseParallelTree<tree>( TI->right, deep + 1 );
    return;
}

template<typename tree>
void checkTreeSameSequential( typename tree::node* T, int dim, const int& DIM ) {
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
size_t checkTreesSize( typename tree::node* T ) {
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
void buildTree( const int& Dim, const parlay::sequence<point>& WP, const int& rounds,
                ParallelKDtree<point>& pkd ) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;

    size_t n = WP.size();
    points wp = points::uninitialized( n );

    double aveBuild = time_loop(
        rounds, 1.0, [&]() { parlay::copy( WP.cut( 0, n ), wp.cut( 0, n ) ); },
        [&]() { pkd.build( wp.cut( 0, n ), Dim ); }, [&]() { pkd.delete_tree(); } );

    LOG << aveBuild << " " << std::flush;

    //* return a built tree
    parlay::copy( WP.cut( 0, n ), wp.cut( 0, n ) );
    pkd.build( wp.cut( 0, n ), Dim );

    return;
}

template<typename point>
void batchInsert( ParallelKDtree<point>& pkd, const parlay::sequence<point>& WP,
                  const parlay::sequence<point>& WI, const uint_fast8_t& DIM,
                  const int& rounds ) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    points wp = points::uninitialized( WP.size() );
    points wi = points::uninitialized( WI.size() );

    pkd.delete_tree();

    // double aveInsert = time_loop(
    //     rounds, 1.0,
    //     [&]() {
    //         parlay::copy( WP, wp ), parlay::copy( WI, wi );
    //         pkd.build( parlay::make_slice( wp ), DIM );
    //     },
    //     [&]() { pkd.batchInsert( parlay::make_slice( wi ), DIM ); },
    //     [&]() { pkd.delete_tree(); } );

    //* set status to be finish insert
    parlay::copy( WP, wp ), parlay::copy( WI, wi );
    pkd.build( parlay::make_slice( wp ), DIM );
    pkd.batchInsert( parlay::make_slice( wi ), DIM );

    // LOG << aveInsert << " " << std::flush;

    return;
}

template<typename point>
void queryKNN( const uint_fast8_t& Dim, const parlay::sequence<point>& WP,
               const int& rounds, ParallelKDtree<point>& pkd, Typename* kdknn,
               const int& K ) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    size_t n = WP.size();
    int LEAVE_WRAP = 32;

    kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[n];
    for ( int i = 0; i < n; i++ ) {
        bq[i].resize( K );
    }
    parlay::sequence<double> visNum = parlay::sequence<double>::uninitialized( n );

    node* KDParallelRoot = pkd.get_root();
    double aveQuery = time_loop(
        rounds, 1.0,
        [&]() { parlay::parallel_for( 0, n, [&]( size_t i ) { bq[i].reset(); } ); },
        [&]() {
            double aveVisNum = 0.0;
            parlay::parallel_for( 0, n, [&]( size_t i ) {
                size_t visNodeNum = 0;
                pkd.k_nearest( KDParallelRoot, WP[i], Dim, bq[i], visNodeNum );
                kdknn[i] = bq[i].top();
                visNum[i] = ( 1.0 * visNodeNum ) / n;
            } );
        },
        [&]() {} );

    LOG << aveQuery << " " << std::flush;

    aveDeep = 0.0;
    traverseParallelTree<tree>( KDParallelRoot, 1 );
    LOG << aveDeep / ( n / LEAVE_WRAP ) << " " << parlay::reduce( visNum.cut( 0, n ) )
        << std::flush;

    return;
}
