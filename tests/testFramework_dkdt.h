#pragma once

#include <iostream>
#include <random>
#include <utility>

#include "common/geometryIO.h"
#include "common/parse_command_line.h"
#include "common/time_loop.h"

#include "parlay/random.h"

// #include "../external/ParGeo/include/kdTree/kdTree.h"
#include "../external/ParGeo/include/pargeo/pointIO.h"

#include "dynamicKdTree/dynKdTree.h"

using coord = long;
using Typename = coord;

double aveDeep = 0.0;

//*---------- generate points within a 0-box_size --------------------
template<typename point>
void
generate_random_points( parlay::sequence<point>& wp, coord _box_size, long n, int Dim) {
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
          wp[i][j] = dis( r );
        }
      },
      1000 );
  return;
}

template<typename point>
std::pair<size_t, int>
read_points( const char* iFile, parlay::sequence<point>& wp, int K ) {
  using coord = std::remove_reference_t<decltype(*std::declval<point>().coords())>; // *
  parlay::sequence<char> S = readStringFromFile( iFile );
  parlay::sequence<char*> W = stringToWords( S );
  size_t N = atol( W[0] );
  int Dim = atoi( W[1] );
  assert( N >= 0 && Dim >= 1 && N >= K );

  auto pts = W.cut( 2, W.size() );
  assert( pts.size() % Dim == 0 );
  size_t n = pts.size() / Dim;
  auto a = parlay::tabulate( Dim * n, [&]( size_t i ) -> coord {
    if constexpr ( std::is_integral_v<coord> )
      return atol( pts[i] );
    else if ( std::is_floating_point_v<coord> )
      return atof( pts[i] );
  } );
  wp.resize( N );
  parlay::parallel_for( 0, n, [&]( size_t i ) {
    for ( int j = 0; j < Dim; j++ ) {
      wp[i][j] = a[i * Dim + j];
    }
  } );
  return std::make_pair( N, Dim );
}

template<class TreeDesc, typename point>
auto
buildTree(const int &Dim, const parlay::sequence<point>& WP, int rounds, size_t leaf_size) {
  // using tree = ParallelKDtree<point>;
  // using points = typename tree::points;
  using points = parlay::sequence<point>;
  // using node = typename tree::node;
  using Tree = typename TreeDesc::type;

  size_t n = WP.size();
  // points wp = points::uninitialized( n );
  parlay::sequence<point> wp( n );
  Tree *tree = nullptr;

  auto prologue = [&]{
    parlay::copy( WP.cut( 0, n ), wp.cut( 0, n ) );
  };
  auto body = [&]{
    // tree = pargeo::kdTree::build<point::dim,point>(wp.cut(0,n), true, leaf_size);
    tree = new Tree(wp);
  };
  auto epilogue = [&]{
    delete tree;
  };

  double aveBuild = time_loop(
      rounds, 1.0, 
      prologue, body, epilogue
  );

  std::cout << aveBuild << " " << std::flush;

  //* return a built tree
  prologue();
  body();
  return tree;
}

template<class TreeDesc, typename point>
void
batchInsert( typename TreeDesc::type *&tree, const parlay::sequence<point>& WP,
             const parlay::sequence<point>& WI, const uint_fast8_t& DIM,
             const int& rounds ) {
  // using tree = ParallelKDtree<point>;
  using points = parlay::sequence<point>;
  // using node = pargeo::kdTree::node<DIM, point>;
  using Tree = typename TreeDesc::type;

  points wp = points::uninitialized( WP.size() );
  points wi = points::uninitialized( WI.size() );

  delete tree;

  auto prologue = [&]{
    parlay::copy( WP, wp ), parlay::copy( WI, wi );
    tree = new Tree(wp);
  };
  auto body = [&]{
    tree->insert( wi );
  };
  auto epilogue = [&]{
    delete tree;
  };

  double aveInsert = time_loop(
      rounds, 1.0,
      prologue, body, epilogue
  );

  std::cout << aveInsert << " " << std::flush;

  //* set status to be finish insert
  prologue();
  body();
}

template<class TreeDesc, typename point>
void
batchDelete( typename TreeDesc::type *&tree, const parlay::sequence<point>& WP,
             const parlay::sequence<point>& WI, const uint_fast8_t& DIM,
             const int& rounds, bool afterInsert = 1 ) {
  // using tree = ParallelKDtree<point>;
  using points = parlay::sequence<point>;
  // using node = typename tree::node;
  using Tree = typename TreeDesc::type;

  points wp = points::uninitialized( WP.size() );
  points wi = points::uninitialized( WI.size() );

  delete tree;

  auto prologue = [&]{
    if ( afterInsert ) {
      parlay::copy( WP, wp ), parlay::copy( WI, wi );
      tree = new Tree(wp);
      tree->insert( wi );
      parlay::copy( WP, wp ), parlay::copy( WI, wi );
    } else {
      wp.resize( WP.size() + WI.size() );
      parlay::copy( parlay::make_slice( WP ), wp.cut( 0, WP.size() ) );
      parlay::copy( parlay::make_slice( WI ), wp.cut( WP.size(), wp.size() ) );
      tree = new Tree( wp );
      parlay::copy( WI, wi );
    }
  };
  auto body = [&]{
    tree->erase( wi );
  };
  auto epilogue = [&]{
    delete tree;
  };

  double aveDelete = time_loop(
      rounds, 1.0,
      prologue, body, epilogue
  );

  std::cout << aveDelete << " " << std::flush;

  //* set status to be finish delete
  prologue();
  body();
}

template<class TreeDesc, typename point>
void
queryKNN( const uint_fast8_t &Dim, const parlay::sequence<point>& WP, const int& rounds,
          typename TreeDesc::type *&tree, Typename* kdknn, const int& K,
          const bool checkCorrect ) {
  // using tree = ParallelKDtree<point>;
  using points = parlay::sequence<point>;
  // using node = typename tree::node;
  // using node = pargeo::kdTree::node<point::dim, point>;
  using Tree = typename TreeDesc::type;
  // using coord = typename point::coord;
  using nn_pair = std::pair<point*, coord>;

  size_t n = WP.size();
  int LEAVE_WRAP = 32;

  points wp = points::uninitialized( n );
  parlay::copy( WP, wp );

  auto test_knn = [&](auto knn_func){
    try{
    double aveQuery = time_loop(
        rounds, 1.0,
        []{}, knn_func, []{}
    );
    std::cout << aveQuery << " " << std::flush;
    }catch(...){
    std::cout << "E " << std::flush;
    }
  };

  test_knn([&]{
    parlay::parallel_for(0, n, [&](size_t i){
      tree->kNN(wp[i], K);
    });
  });

  // std::cout << "dualknn " << std::flush;
  // test_knn([&]{pargeo::batchKdTree::dualKnn(wp,*tree,K);});
}

template<class TreeDesc, typename point>
void
rangeCount( const parlay::sequence<point>& wp, typename TreeDesc::type *&tree,
            Typename* kdknn, const int& rounds, const int& queryNum ) {
  // using tree = ParallelKDtree<point>;
  using Tree = typename TreeDesc::type;
  // using points = typename tree::points;
  using points = parlay::sequence<point>;
  // using node = typename tree::node;
  // using box = typename tree::box;

  std::cout << "-1 " << std::flush;
  /*
  int n = wp.size();
  double aveCount = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
          box queryBox = tree.get_box(
              box( wp[i], wp[i] ), box( wp[( i + n / 2 ) % n], wp[( i + n / 2 ) % n] ) );
          kdknn[i] = tree.range_count( queryBox );
        } );
      },
      [&]() {} );

  std::out << aveCount << " " << std::flush;
  */
}

template<class TreeDesc, typename point>
void
rangeQuery( const parlay::sequence<point>& wp, typename TreeDesc::type *&tree,
            Typename* kdknn, const int& rounds, const int& queryNum,
            parlay::sequence<point>& Out ) {
  // using tree = ParallelKDtree<point>;
  using Tree = typename TreeDesc::type;
  // using points = typename tree::points;
  // using node = typename tree::node;
  // using box = typename tree::box;
  using points = parlay::sequence<point>;

  std::cout << "-1 " << std::flush;
  /*
  int n = wp.size();
  size_t step = Out.size() / queryNum;

  parlay::sequence<points> res(queryNum);

  double aveQuery = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
          point qMin=wp[i], qMax=qMin, q=wp[(i+n/2)%n];
          qMin.minCoords(q);
          qMax.maxCoords(q);
          res[i] = tree->orthogonalQuery(qMin, qMax);
        } );
      },
      [&]() {} );

  std::cout << aveQuery << " " << std::flush;
  */
}
