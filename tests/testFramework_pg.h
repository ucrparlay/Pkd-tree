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

#include "batchKdtree/cache-oblivious/cokdtree.h"
#include "batchKdtree/binary-heap-layout/bhlkdtree.h"
#include "batchKdtree/log-tree/logtree.h"
#include "batchKdtree/shared/dual.h"

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
  using coord = std::remove_reference_t<decltype(*std::declval<point>().coordinate())>; // *
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
/*
template<typename tree>
void
traverseParallelTree( typename tree::node* root, int deep ) {
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
void
checkTreeSameSequential( typename tree::node* T, int dim, const int& DIM ) {
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
size_t
checkTreesSize( typename tree::node* T ) {
  if ( T->is_leaf ) {
    return T->size;
  }
  typename tree::interior* TI = static_cast<typename tree::interior*>( T );
  size_t l = checkTreesSize<tree>( TI->left );
  size_t r = checkTreesSize<tree>( TI->right );
  assert( l + r == T->size );
  return T->size;
}
*/
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
    const auto &cwp = wp;
    tree = new Tree(cwp.cut(0,n));
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
  if(!TreeDesc::support_insert)
  {
    std::cout << "-1 " << std::flush;
    return;
  }

  // using tree = ParallelKDtree<point>;
  using points = parlay::sequence<point>;
  // using node = pargeo::kdTree::node<DIM, point>;
  using Tree = typename TreeDesc::type;

  points wp = points::uninitialized( WP.size() );
  points wi = points::uninitialized( WI.size() );

  delete tree;

  auto prologue = [&]{
    parlay::copy( WP, wp ), parlay::copy( WI, wi );
    const auto &cwp = wp;
    // tree = new Tree( parlay::make_slice( cwp ) );
    const int log2size = (int)std::ceil(std::log2(WP.size()+WI.size()));
    tree = new Tree(log2size);
    tree->insert( parlay::make_slice( cwp ) );
  };
  auto body = [&]{
    const auto &cwi = wi;
    tree->insert( parlay::make_slice( cwi ) );
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
  if(afterInsert && !TreeDesc::support_insert_delete)
  {
    std::cout << "-1 " << std::flush;
    return;
  }

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
      const auto &cwp=wp, &cwi = wi;
      // tree = new Tree( parlay::make_slice( cwp ) );
      // tree->insert( parlay::make_slice( cwi ) );
      const int log2size = (int)std::ceil(std::log2(WP.size()+WI.size()));
      tree = new Tree(log2size);
      tree->insert( parlay::make_slice( cwp ) );
      tree->insert( parlay::make_slice( cwi ) );
      parlay::copy( WP, wp ), parlay::copy( WI, wi );
    } else {
      wp.resize( WP.size() + WI.size() );
      parlay::copy( parlay::make_slice( WP ), wp.cut( 0, WP.size() ) );
      parlay::copy( parlay::make_slice( WI ), wp.cut( WP.size(), wp.size() ) );
      const auto &cwp = wp;
      tree = new Tree( parlay::make_slice( cwp ) );
      parlay::copy( WI, wi );
    }
  };
  auto body = [&]{
    tree->bulk_erase( wi );
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
    double aveQuery = time_loop(
        rounds, 1.0,
        []{}, knn_func, []{}
    );
    std::cout << aveQuery << " " << std::flush;
  };

  std::cout << "knn1 " << std::flush;
  test_knn([&]{tree->template knn<false,false>(wp,K);});
  test_knn([&]{tree->template knn<false,true>(wp,K);});
  test_knn([&]{tree->template knn<true,false>(wp,K);});
  test_knn([&]{tree->template knn<true,true>(wp,K);});

  if(TreeDesc::support_knn2)
  {
    std::cout << "knn2 " << std::flush;
    test_knn([&]{tree->template knn2<false,false>(wp,K);});
    test_knn([&]{tree->template knn2<false,true>(wp,K);});
    test_knn([&]{tree->template knn2<true,false>(wp,K);});
    test_knn([&]{tree->template knn2<true,true>(wp,K);});
  }

  if(TreeDesc::support_knn3)
  {
    std::cout << "knn3 " << std::flush;
    test_knn([&]{tree->template knn3<false,false>(wp,K);});
    test_knn([&]{tree->template knn3<false,true>(wp,K);});
    test_knn([&]{tree->template knn3<true,false>(wp,K);});
    test_knn([&]{tree->template knn3<true,true>(wp,K);});
  }

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

  int n = wp.size();
  size_t step = Out.size() / queryNum;

  parlay::sequence<points> res(queryNum);

  double aveQuery = time_loop(
      rounds, 1.0, [&]() {},
      [&]() {
        parlay::parallel_for( 0, queryNum, [&]( size_t i ) {
          /*
          box queryBox = tree.get_box(
              box( wp[i], wp[i] ), box( wp[( i + n / 2 ) % n], wp[( i + n / 2 ) % n] ) );
          kdknn[i] = tree.range_query( queryBox, Out.cut( i * step, ( i + 1 ) * step ) );
          */
          point qMin=wp[i], qMax=qMin, q=wp[(i+n/2)%n];
          qMin.minCoords(q);
          qMax.maxCoords(q);
          res[i] = tree->orthogonalQuery(qMin, qMax);
        } );
      },
      [&]() {} );

  std::cout << aveQuery << " " << std::flush;
}
/*
template<typename point>
void
generate_knn( const uint_fast8_t& Dim, const parlay::sequence<point>& WP,
              const int& rounds, ParallelKDtree<point>& pkd, Typename* kdknn,
              const int& K, const bool checkCorrect, const char* outFile ) {
  using tree = ParallelKDtree<point>;
  using points = typename tree::points;
  using node = typename tree::node;
  using coord = typename point::coord;
  using nn_pair = std::pair<point*, coord>;
  size_t n = WP.size();
  int LEAVE_WRAP = 32;

  parlay::sequence<nn_pair> Out( K * n );
  parlay::sequence<kBoundedQueue<point>> bq =
      parlay::sequence<kBoundedQueue<point>>::uninitialized( n );
  parlay::parallel_for(
      0, n, [&]( size_t i ) { bq[i].resize( Out.cut( i * K, i * K + K ) ); } );

  node* KDParallelRoot = pkd.get_root();
  points wp = points::uninitialized( n );
  parlay::copy( WP, wp );
  parlay::parallel_for( 0, n, [&]( size_t i ) { bq[i].reset(); } );

  double aveVisNum = 0.0;
  parlay::parallel_for( 0, n, [&]( size_t i ) {
    size_t visNodeNum = 0;
    pkd.k_nearest( KDParallelRoot, wp[i], Dim, bq[i], visNodeNum );
  } );

  std::ofstream ofs( outFile );
  if ( !ofs.is_open() ) {
    throw( "file not open" );
    abort();
  }
  size_t m = n * K;
  ofs << "WeightedAdjacencyGraph" << '\n';
  ofs << n << '\n';
  ofs << m << '\n';
  parlay::sequence<uint64_t> offset( n + 1 );
  parlay::parallel_for( 0, n + 1, [&]( size_t i ) { offset[i] = i * K; } );
  // parlay::parallel_for( 0, n, [&]( size_t i ) {
  //   for ( size_t j = 0; j < K; j++ ) {
  //     if ( Out[i * K + j].first == wp[i] ) {
  //       printf( "%d, self-loop\n", i );
  //       exit( 0 );
  //     }
  //   }
  // } );
  parlay::sequence<point> edge( m );
  parlay::parallel_for( 0, m, [&]( size_t i ) { edge[i] = *( Out[i].first ); } );
  parlay::sequence<double> weight( m );
  parlay::parallel_for( 0, m, [&]( size_t i ) { weight[i] = Out[i].second; } );
  for ( size_t i = 0; i < n; i++ ) {
    ofs << offset[i] << '\n';
  }
  for ( size_t i = 0; i < m; i++ ) {
    for ( auto j : edge[i].pnt )
      ofs << j << " ";
    ofs << "\n";
    // ofs << edge[i] << '\n';
  }
  for ( size_t i = 0; i < n; i++ ) {
    ofs << weight[i] << '\n';
  }
  ofs.close();
  return;
}
*/
