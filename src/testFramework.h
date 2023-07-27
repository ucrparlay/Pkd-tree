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
void read_points( const char* iFile, parlay::sequence<point>& wp, size_t& N, int& Dim,
                  int K ) {
  parlay::sequence<char> S = readStringFromFile( iFile );
  parlay::sequence<char*> W = stringToWords( S );
  N = atol( W[0] ), Dim = atoi( W[1] );
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
  return;
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
void buildTree( const int& Dim, const typename tree::points& wp, const int& rounds,
                tree& pkd ) {
  using points = typename tree::points;
  using node = typename tree::node;
  points wx;
  size_t n = wp.size();

  double aveBuild = time_loop(
      rounds, 1.0,
      [&]() {
        wx = points::uninitialized( n );
        parlay::copy( parlay::random_shuffle( wp.cut( 0, n ) ), wx.cut( 0, n ) );
      },
      [&]() { pkd.build( wx.cut( 0, n ), Dim ); },
      [&]() {
        if ( pkd.get_root() != NULL ) pkd.delete_tree( pkd.get_root() );
      } );

  LOG << aveBuild << " " << std::flush;
  return;
}

template<typename tree>
void queryKNN( const uint_fast8_t& Dim, const typename tree::points& wp,
               const int& rounds, tree& pkd, Typename* kdknn, const int& K ) {
  using points = typename tree::points;
  using node = typename tree::node;
  size_t n = wp.size();
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
          pkd.k_nearest( KDParallelRoot, wp[i], Dim, bq[i], visNodeNum );
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

template<typename tree>
void batchInsert( tree& pkd, const typename tree::points& wi, const uint_fast8_t& DIM,
                  const int& rounds, const size_t& len ) {
  using points = typename tree::points;
  using node = typename tree::node;
  size_t n = wi.size();
  points wx = points::uninitialized( n );

  size_t s = 0;

  double aveInsert = time_loop(
      rounds, 1.0,
      [&]() {
        parlay::copy( parlay::random_shuffle( wi.cut( s, s + len ) ),
                      wx.cut( s, s + len ) );
      },
      [&]() {
        pkd.batchInsert( wx.cut( s, s + len ), DIM );
        s += len;
      },
      [&]() {} );

  LOG << aveInsert << " " << std::flush;

  return;
}