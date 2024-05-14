#pragma once

#include <iostream>
#include <random>
#include <utility>
#include <tuple>

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
      rounds, -1.0, 
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
      rounds, -1.0,
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
             const int& rounds, bool afterInsert = 1, bool disableInsertion=false) {
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
      if(!disableInsertion)
        tree->insert( parlay::make_slice( cwi ) );
      parlay::copy( WP, wp ), parlay::copy( WI, wi );
    } else {
      if(!disableInsertion)
      {
        wp.resize( WP.size() + WI.size() );
        parlay::copy( parlay::make_slice( WP ), wp.cut( 0, WP.size() ) );
        parlay::copy( parlay::make_slice( WI ), wp.cut( WP.size(), wp.size() ) );
      }
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
      rounds, -1.0,
      prologue, body, epilogue
  );

  std::cout << aveDelete << " " << std::flush;

  //* set status to be finish delete
  prologue();
  body();
}

template<class TreeDesc, typename point>
void batchInsertDelete(typename TreeDesc::type *&tree,
  const parlay::sequence<point>& WI,
  const uint_fast8_t& DIM, const int& rounds ) {
  for(int i=0; i<rounds; ++i)
  {
    tree->insert(parlay::make_slice(WI));
    auto wd = WI;
    tree->bulk_erase(wd);
  }
}

template<class TreeDesc, typename point>
void
queryKNN( const uint_fast8_t &Dim, const parlay::sequence<point>& WP, const int& rounds,
          typename TreeDesc::type *tree, Typename* kdknn, const int& K,
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

  if(TreeDesc::support_knn1)
  {
    std::cout << "knn1 " << std::flush;
    //test_knn([&]{tree->template knn<false,false>(wp,K);});
    //test_knn([&]{tree->template knn<false,true>(wp,K);});
    test_knn([&]{tree->template knn<true,false>(wp,K);});
    //test_knn([&]{tree->template knn<true,true>(wp,K);});
  }
  if(TreeDesc::support_knn2)
  {
    std::cout << "knn2 " << std::flush;
    // test_knn([&]{tree->template knn2<false,false>(wp,K);});
    // test_knn([&]{tree->template knn2<false,true>(wp,K);});
    test_knn([&]{tree->template knn2<true,false>(wp,K);});
    // test_knn([&]{tree->template knn2<true,true>(wp,K);});
  }

  if(TreeDesc::support_knn3)
  {
    std::cout << "knn3 " << std::flush;
    // test_knn([&]{tree->template knn3<false,false>(wp,K);});
    // test_knn([&]{tree->template knn3<false,true>(wp,K);});
    test_knn([&]{tree->template knn3<true,false>(wp,K);});
    // test_knn([&]{tree->template knn3<true,true>(wp,K);});
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

//* [a,b)
size_t
get_random_index( size_t a, size_t b, int seed ) {
  return size_t( ( rand() % ( b - a ) ) + a );
  // return size_t( ( parlay::hash64( static_cast<uint64_t>( seed ) ) % ( b - a ) ) + a );
}

template<typename point>
size_t
recurse_box( parlay::slice<point*, point*> In,
             parlay::sequence<std::tuple<point,point,int>>& boxs, int DIM,
             std::pair<size_t, size_t> range, int& idx, int recNum, int type ) {
  // using tree = ParallelKDtree<point>;

  size_t n = In.size();
  if ( idx >= recNum || n < range.first || n == 0 ) return 0;

  size_t mx = 0;
  bool goon = false;

  if (n <= range.second) {
    point qMin = parlay::reduce(
      In, 
      parlay::binary_op([](const point &lhs, const point &rhs){
        point t = lhs;
        t.minCoords(rhs);
        return t;
      }, point{})
    );

    point qMax = parlay::reduce(
      In, 
      parlay::binary_op([](const point &lhs, const point &rhs){
        point t = lhs;
        t.maxCoords(rhs);
        return t;
      },point{})
    );

    boxs[idx++] = std::make_tuple(qMin, qMax, n);

    // NOTE: handle the cose that all points are the same then become un-divideable
    // NOTE: Modify the coefficient to make the rectangle size distribute as uniform as possible within the range
    if ((type == 0 && n < 40 * range.first) || (type == 1 && n < 10 * range.first) ||
        (type == 2 && n < 2 * range.first) || parlay::all_of(In, [&](const point& p) { return p == In[0]; })) {
      return In.size();
    } else {
      goon = true;
      mx = n;
    }
  }

  int dim = get_random_index( 0, DIM, rand() );
  size_t pos = get_random_index( 0, n, rand() );
  parlay::sequence<bool> flag( n, 0 );
  parlay::parallel_for( 0, n, [&]( size_t i ) {
    if ( In[i][dim] > In[pos][dim] )
      flag[i] = 1;
    else
      flag[i] = 0;
  } );
  auto [Out, m] = parlay::internal::split_two( In, flag );

  assert( Out.size() == n );
  // LOG << dim << " " << Out[0] << Out[m] << ENDL;
  size_t l, r;
  l = recurse_box<point>( Out.cut( 0, m ), boxs, DIM, range, idx, recNum, type );
  r = recurse_box<point>( Out.cut( m, n ), boxs, DIM, range, idx, recNum, type );

  if ( goon ) {
    return mx;
  } else {
    return std::max( l, r );
  }
}

template<typename point>
std::pair<parlay::sequence<std::tuple<point,point,int>>, size_t>
gen_rectangles( int recNum, const int type, const parlay::sequence<point>& WP, int DIM ) {
  // using tree = ParallelKDtree<point>;
  // using Tree = typename TreeDesc::type;
  // using points = typename tree::points;
  using points = parlay::sequence<point>;
  // using node = typename tree::node;
  using box = std::tuple<point, point, int>;
  using boxs = parlay::sequence<box>;

  size_t n = WP.size();
  std::pair<size_t, size_t> range;
  if ( type == 0 ) {  //* small bracket
    range.first = 1;
    range.second = size_t( std::sqrt( std::sqrt( n ) ) );
  } else if ( type == 1 ) {  //* medium bracket
    range.first = size_t( std::sqrt( std::sqrt( n ) ) );
    range.second = size_t( std::sqrt( n ) );
  } else {  //* large bracket
    range.first = size_t( std::sqrt( n ) );

    // NOTE: special handle for duplicated points in 2-dimension varden
    if ( n >= 100000000 )
      range.second = n / 100 - 1;
    else if (n == 1000000000)
      range.second = n / 1000 - 1;
    else
      range.second = n - 1;
  }

  boxs bxs( recNum );
  int cnt = 0;
  points wp( n );

  srand( 10 );

  size_t maxSize = 0;
  while ( cnt < recNum ) {
    parlay::copy( WP, wp );
    auto r = recurse_box<point>( parlay::make_slice( wp ), bxs, DIM, range, cnt, recNum,
                                 type );
    maxSize = std::max( maxSize, r );
    // LOG << cnt << " " << maxSize << ENDL;
  }
  // LOG << "finish generate " << ENDL;
  return std::make_pair( bxs, maxSize );
}

template<class TreeDesc, typename point>
void
rangeQuery( const parlay::sequence<point>& wp, typename TreeDesc::type *&tree, int dim,
            int rounds, int num_rect, int type_rect) {
  // using tree = ParallelKDtree<point>;
  using Tree = typename TreeDesc::type;
  // using points = typename tree::points;
  // using node = typename tree::node;
  // using box = typename tree::box;
  using points = parlay::sequence<point>;

  auto [rects, maxsize] = gen_rectangles(num_rect, type_rect, wp, dim);
  assert(rects.size()==num_rect || (std::cerr<<"rects.size: "<<rects.size()<<'\n',false));

  /*
  auto test_rangeQuery = [&](float rec_ratio){
    size_t res_size = 0;
    double aveQuery = time_loop(
      rounds, 1.0, []{},
      [&](){
        point qUpper = qMin + qDiff*rec_ratio;
        res_size = tree->orthogonalQuery(qMin, qUpper).size();
      },
      []{}
    );
    std::cout << rec_ratio << ' ' << aveQuery << ' ' << res_size << " | " << std::flush;
  };

  float ratio[] = {0.05, 0.1, 0.2, 0.3, 0.5, 0.7, 0.9, 1.01};
  for(size_t i=0; i<sizeof(ratio)/sizeof(*ratio); ++i)
    test_rangeQuery(ratio[i]);
  */
  double aveQuery = time_loop(
    rounds, 1.0, []{},
    [&](){
      parlay::parallel_for(0, rects.size(), [&](size_t i){
        const auto &[qMin, qMax, num_in_rect] = rects[i];
        auto res = tree->orthogonalQuery(qMin, qMax);

        if(res.size()!=num_in_rect) throw "num_in_rect doesn't match";
        point resMin = parlay::reduce(
          res, 
          parlay::binary_op([](const point &lhs, const point &rhs){
            point t = lhs;
            t.minCoords(rhs);
            return t;
          }, point{})
        );
        point resMax = parlay::reduce(
          res, 
          parlay::binary_op([](const point &lhs, const point &rhs){
            point t = lhs;
            t.maxCoords(rhs);
            return t;
          },point{})
        );
        if(resMin!=qMin) throw "wrong min point";
        if(resMax!=qMax) throw "wrong max point";
      });
    },
    []{}
  );
  std::cout << aveQuery << ' ' << std::flush;
}

template<class TreeDesc, typename point>
void rangeQueryWithLog(const parlay::sequence<point>& wp, typename TreeDesc::type *tree,
                             const int& rounds, int type_rect, int num_rect, int dim){
    // using tree = ParallelKDtree<point>;
    using Tree = typename TreeDesc::type;
    // using points = typename tree::points;
    // using node = typename tree::node;
    // using box = typename tree::box;
    using points = parlay::sequence<point>;

    auto [rects, maxsize] = gen_rectangles(num_rect, type_rect, wp, dim);

    for(const auto &rect : rects){
        const auto &[qMin, qMax, num_in_rect] = rect;
        points res;
        auto time_avg = time_loop(
            rounds, -1.0,
            []{},
            [&](){res = tree->orthogonalQuery(qMin, qMax);},
            []{}
        );
        std::cout << num_in_rect << " " << res.size() << " " << time_avg << std::endl;
    }
}

template<class TreeDesc, typename point>
void insertOsmByTime(
    int Dim, const parlay::sequence<parlay::sequence<point>>& node_by_time,
    int rounds, typename TreeDesc::type *&tree, 
    const parlay::sequence<point> &qs, int K) 
{
    // using tree = ParallelKDtree<point>;
    using points = parlay::sequence<point>;
    // using node = typename tree::node;
    using Tree = typename TreeDesc::type;

    size_t time_period_num = node_by_time.size();
    size_t size_total = 0;
    parlay::sequence<points> wp(time_period_num);
    for(size_t i = 0; i < time_period_num; i++){
      size_t size_cur = node_by_time[i].size();
      wp[i].resize(size_cur);
      size_total += size_cur;
    }
    const int log2size = (int)std::ceil(std::log2(size_total));

    auto prologue = [&](){
        for(size_t i = 0; i < time_period_num; i++){
            parlay::copy(node_by_time[i], wp[i]);
        }
        tree = new Tree(log2size);
    };
    auto body = [&](){
        for(size_t i = 0; i < time_period_num; i++){
          tree->insert(parlay::make_slice(std::as_const(wp[i])));
        }
    };
    auto epilogue = [&](){
      delete tree;
    };

    double ave = time_loop(
        rounds, -1.0,
        prologue, body, epilogue
    );

    std::cout << ave << " " << std::endl;

    prologue();
    for(size_t i = 0; i < time_period_num; i++){
        parlay::internal::timer t;
        t.reset(), t.start();
        tree->insert(parlay::make_slice(std::as_const(wp[i])));
        t.stop();
        queryKNN<TreeDesc,point>(Dim, qs, rounds, tree, nullptr, K, false );
        std::cout << wp[i].size() << " " << t.total_time() << std::endl;
    }
    std::cout << std::endl;
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
