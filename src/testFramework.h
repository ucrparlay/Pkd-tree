#pragma once

#include "common/geometryIO.h"
#include "common/parse_command_line.h"
#include "common/time_loop.h"
#include "kdTree.h"
#include "kdTreeParallel.h"

using Typename = coord;

double aveDeep = 0.0;

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
    return;
  }
  typename tree::interior* TI = static_cast<typename tree::interior*>( T );
  assert( TI->split.second == dim );
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
      [&]() { pkd.start_build( wx.cut( 0, n ), Dim ); },
      [&]() { pkd.delete_tree( pkd.get_root() ); } );

  LOG << aveBuild << " " << std::flush;
  return;
}

template<typename tree>
void queryKNN( const int& Dim, const typename tree::points& wp, const int& rounds,
               tree& pkd, Typename* kdknn, const int& K ) {
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
