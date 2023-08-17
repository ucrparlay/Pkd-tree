#pragma once

#include "utility.h"

#define LOG  std::cout
#define ENDL std::endl << std::flush

template<typename point>
class ParallelKDtree {
 public:
  using slice = parlay::slice<point*, point*>;
  using points = parlay::sequence<point>;
  using coords = typename point::coords;
  //@ take the value of a point in specific dimension
  using splitter = std::pair<coord, uint_fast8_t>;
  using splitter_s = parlay::sequence<splitter>;
  using box = std::pair<point, point>;
  using box_s = parlay::sequence<box>;

  struct node {
    bool is_leaf;
    size_t size;
    uint_fast8_t dim;
    node* parent;
  };

  struct leaf : node {
    points pts;
    leaf( slice In, const uint_fast8_t& dim ) :
        node{ true, static_cast<size_t>( In.size() ), dim, nullptr } {
      pts = points::uninitialized( LEAVE_WRAP );
      for ( int i = 0; i < In.size(); i++ ) {
        pts[i] = In[i];
      }
    }
  };

  struct interior : node {
    node* left;
    node* right;
    splitter split;
    interior( node* _left, node* _right, splitter _split, const uint_fast8_t& dim ) :
        node{ false, _left->size + _right->size, dim, nullptr },
        left( _left ),
        right( _right ),
        split( _split ) {
      left->parent = this;
      right->parent = this;
    }
  };

  enum split_rule { MAX_STRETCH_DIM, ROTATE_DIM };

 private:
  node* root = nullptr;
  parlay::internal::timer timer;
  split_rule _split_rule = MAX_STRETCH_DIM;
  box bbox;

 public:
  size_t rebuild_time = 0;

 public:
  //@ tag each node an bucket id
  using node_tag = std::pair<node*, int_fast32_t>;
  using node_tags = parlay::sequence<node_tag>;
  using tag_nodes = parlay::sequence<uint_fast32_t>;  //*index by tag

  //@ Const variables
  //@ uint32t handle up to 4e9 at least
  static constexpr uint_fast32_t BUILD_DEPTH_ONCE = 6;  //* last layer is leaf
  static constexpr uint_fast32_t PIVOT_NUM = ( 1 << BUILD_DEPTH_ONCE ) - 1;
  static constexpr uint_fast32_t BUCKET_NUM = 1 << BUILD_DEPTH_ONCE;
  //@ general
  static constexpr uint_fast32_t LEAVE_WRAP = 32;
  static constexpr uint_fast32_t THIN_LEAVE_WRAP = 24;
  static constexpr uint_fast32_t SERIAL_BUILD_CUTOFF = 1 << 10;
  //@ block param in partition
  static constexpr uint_fast32_t LOG2_BASE = 10;
  static constexpr uint_fast32_t BLOCK_SIZE = 1 << LOG2_BASE;
  //@ reconstruct ratio
  static constexpr uint_fast8_t INBALANCE_RATIO = 20;

 public:
  struct pointLess {
    pointLess( size_t index ) : index_( index ) {}
    bool
    operator()( const point& p1, const point& p2 ) const {
      return p1.pnt[index_] < p2.pnt[index_];
    }
    size_t index_;
  };

  struct InnerTree {
    //@ helpers
    bool
    assert_size( node* T ) const {
      if ( T->is_leaf ) {
        leaf* TI = static_cast<leaf*>( T );
        assert( T->size == TI->pts.size() && T->size <= LEAVE_WRAP );
        return true;
      }
      interior* TI = static_cast<interior*>( T );
      assert( TI->size == TI->left->size + TI->right->size );
      return true;
    }

    void
    assert_size_by_idx( int idx ) const {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return;
      interior* TI = static_cast<interior*>( tags[idx].first );
      assert( TI->size == TI->left->size + TI->right->size );
      return;
    }

    inline int
    get_node_idx( int idx, node* T ) {
      if ( tags[idx].first == T ) return idx;
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return -1;
      auto pos = get_node_idx( idx << 1, T );
      if ( pos != -1 ) return pos;
      return get_node_idx( idx << 1 | 1, T );
    }

    //@ cores
    inline void
    reset_tags_num() {
      tagsNum = 0;
    }

    void
    assign_node_tag( node* T, int idx ) {
      if ( T->is_leaf || idx > PIVOT_NUM ) {
        assert( tagsNum < BUCKET_NUM );
        tags[idx] = node_tag( T, tagsNum++ );
        return;
      }
      //* BUCKET ID in [0, BUCKET_NUM)
      tags[idx] = node_tag( T, BUCKET_NUM );
      interior* TI = static_cast<interior*>( T );
      assign_node_tag( TI->left, idx << 1 );
      assign_node_tag( TI->right, idx << 1 | 1 );
      return;
    }

    void
    reduce_sums( int idx ) const {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        assert( tags[idx].second < BUCKET_NUM );
        sums_tree[idx] = sums[tags[idx].second];
        return;
      }
      reduce_sums( idx << 1 );
      reduce_sums( idx << 1 | 1 );
      sums_tree[idx] = sums_tree[idx << 1] + sums_tree[idx << 1 | 1];
      return;
    }

    void
    pick_tag( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        tags[idx].second = BUCKET_NUM + 1;
        rev_tag[tagsNum++] = idx;
        return;
      }
      assert( tags[idx].second == BUCKET_NUM && ( !tags[idx].first->is_leaf ) );
      interior* TI = static_cast<interior*>( tags[idx].first );

      if ( inbalance_node( TI->left->size + sums_tree[idx << 1],
                           TI->size + sums_tree[idx] ) ) {
        tags[idx].second = BUCKET_NUM + 2;
        rev_tag[tagsNum++] = idx;
        return;
      }
      pick_tag( idx << 1 );
      pick_tag( idx << 1 | 1 );
      return;
    }

    void
    tag_inbalance_node() {
      reduce_sums( 1 );
      reset_tags_num();
      pick_tag( 1 );
      assert( assert_size( tags[1].first ) );
      return;
    }

    void
    mark_tomb( int idx, bool hasTomb ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        assert( tags[idx].second >= 0 && tags[idx].second < BUCKET_NUM );
        tags[idx].second = ( !hasTomb ) ? BUCKET_NUM + 2 : BUCKET_NUM + 1;
        rev_tag[tagsNum++] = idx;
        return;
      }
      assert( tags[idx].second == BUCKET_NUM && ( !tags[idx].first->is_leaf ) );
      interior* TI = static_cast<interior*>( tags[idx].first );
      if ( hasTomb && ( inbalance_node( TI->left->size - sums_tree[idx << 1],
                                        TI->size - sums_tree[idx] ) ||
                        ( TI->size - sums_tree[idx] < THIN_LEAVE_WRAP ) ) ) {
        assert( hasTomb != 0 );
        tags[idx].second = BUCKET_NUM + 3;
        hasTomb = false;
      }

      mark_tomb( idx << 1, hasTomb );
      mark_tomb( idx << 1 | 1, hasTomb );
      return;
    }

    void
    tag_inbalance_node_deletion( bool hasTomb ) {
      reduce_sums( 1 );
      reset_tags_num();
      mark_tomb( 1, hasTomb );
      assert( assert_size( tags[1].first ) );
      return;
    }

    void
    init() {
      reset_tags_num();
      tags = node_tags::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
      sums_tree = parlay::sequence<uint_fast32_t>( PIVOT_NUM + BUCKET_NUM + 1 );
      rev_tag = tag_nodes::uninitialized( BUCKET_NUM );
    }

    //@ variables
    // TODO node and tag can be separated with each other, leave node as a delayed may
    node_tags tags;
    parlay::sequence<uint_fast32_t> sums;
    mutable parlay::sequence<uint_fast32_t> sums_tree;
    mutable tag_nodes rev_tag;
    int tagsNum;
  };

  // TODO: handle double precision

  // Given two points, return the min. value on each dimension
  // minv[i] = smaller value of two points on i-th dimension

 public:
  inline void
  set_root( node* _root ) {
    this->root = _root;
  }

  inline node*
  get_root() {
    return this->root;
  }

  //@ Support Functions
  parlay::type_allocator<leaf> leaf_allocator;
  parlay::type_allocator<interior> interior_allocator;

  leaf*
  alloc_leaf_node( slice In, const uint_fast8_t& dim ) {
    leaf* o = parlay::type_allocator<leaf>::alloc();
    new ( o ) leaf( In, dim );
    return o;
  }

  interior*
  alloc_interior_node( node* L, node* R, const splitter& split,
                       const uint_fast8_t& dim ) {
    interior* o = parlay::type_allocator<interior>::alloc();
    new ( o ) interior( L, R, split, dim );
    return o;
  }

  void
  free_leaf( node* T ) {
    parlay::type_allocator<leaf>::retire( static_cast<leaf*>( T ) );
  }

  void
  free_interior( node* T ) {
    parlay::type_allocator<interior>::retire( static_cast<interior*>( T ) );
  }

  static inline bool
  inbalance_node( const size_t& l, const size_t& n ) {
    if ( n == 0 ) return true;
    return Gt( std::abs( 100.0 * l / n - 50.0 ), 1.0 * INBALANCE_RATIO );
  }

  static inline bool
  within_box( const box& a, const box& b ) {
    for ( int i = 0; i < a.first.get_dim(); i++ ) {
      if ( a.first.pnt[i] < b.first.pnt[i] || a.second.pnt[i] > b.second.pnt[i] ) {
        return false;
      }
    }
    return true;
  }

  static box
  get_box( const box& x, const box& y ) {
    return box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) );
  }

  static box
  get_box( slice V ) {
    if ( V.size() == 0 ) {
      return std::move( box() );
    } else if ( V.size() <= SERIAL_BUILD_CUTOFF ) {  // * attention to high dimension
      box b( V[0], V[0] );
      for ( int i = 1; i < V.size(); i++ ) {
        for ( int j = 0; j < V[0].get_dim(); j++ ) {
          b.first.pnt[j] = std::min( b.first.pnt[j], V[i].pnt[j] );
          b.second.pnt[j] = std::max( b.second.pnt[j], V[i].pnt[j] );
        }
      }
      return std::move( b );
    } else {
      auto minmax = [&]( const box& x, const box& y ) {
        return box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) );
      };
      auto boxes = parlay::delayed_seq<box>(
          V.size(), [&]( size_t i ) { return box( V[i].pnt, V[i].pnt ); } );
      return std::move(
          parlay::reduce( boxes, parlay::make_monoid( minmax, boxes[0] ) ) );
    }
  }

  inline uint_fast8_t
  pick_max_stretch_dim( const box& bx, const uint_fast8_t& DIM ) {
    uint_fast8_t d( 0 );
    coord diff( bx.second.pnt[0] - bx.first.pnt[0] );
    assert( diff >= 0 );
    for ( int i = 1; i < DIM; i++ ) {
      if ( bx.second.pnt[i] - bx.first.pnt[i] > diff ) {
        diff = bx.second.pnt[i] - bx.first.pnt[i];
        d = i;
      }
    }
    return d;
  }

  //@ Parallel KD tree cores
  //@ build
  void
  divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim, int idx, int deep,
                 int& bucket, const uint_fast8_t& DIM, box_s& boxs, const box& bx );

  void
  pick_pivots( slice In, const size_t& n, splitter_s& pivots, const uint_fast8_t& dim,
               const uint_fast8_t& DIM, box_s& boxs, const box& bx );

  inline uint_fast32_t
  find_bucket( const point& p, const splitter_s& pivots, const uint_fast8_t& dim,
               const uint_fast8_t& DIM );

  void
  partition( slice A, slice B, const size_t& n, const splitter_s& pivots,
             parlay::sequence<uint_fast32_t>& sums, const uint_fast8_t& dim,
             const uint_fast8_t& DIM );

  node*
  build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                    parlay::sequence<node*>& treeNodes );

  inline coord
  ppDistanceSquared( const point& p, const point& q, const uint_fast8_t& DIM );

  void
  build( slice In, const uint_fast8_t& DIM );

  node*
  build_recursive( slice In, slice Out, uint_fast8_t dim, const uint_fast8_t& DIM,
                   box bx );

  void
  k_nearest( node* T, const point& q, const uint_fast8_t& DIM, kBoundedQueue<coord>& bq,
             size_t& visNodeNum );

  //@ batch insert
  void
  batchInsert( slice In, const uint_fast8_t& DIM );

  void
  flatten( node* T, slice Out );

  void
  flatten_and_delete( node* T, slice Out );

  inline void
  update_interior( node* T, node* L, node* R );

  void
  seieve_points( slice A, slice B, const size_t& n, const node_tags& tags,
                 parlay::sequence<uint_fast32_t>& sums, const int& tagsNum );

  inline uint_fast32_t
  retrive_tag( const point& p, const node_tags& tags );

  node*
  update_inner_tree( uint_fast32_t idx, const node_tags& tags,
                     parlay::sequence<node*>& treeNodes, int& p,
                     const tag_nodes& rev_tag );

  node*
  batchInsert_recusive( node* T, slice In, slice Out, const uint_fast8_t& DIM );

  void
  batchDelete( slice In, const uint_fast8_t& DIM );

  node*
  batchDelete_recursive( node* T, slice In, slice Out, const uint_fast8_t& DIM,
                         bool hasTomb );

  node*
  delete_inner_tree( uint_fast32_t idx, const node_tags& tags,
                     parlay::sequence<node*>& treeNodes, int& p, const tag_nodes& rev_tag,
                     const uint_fast8_t& DIM );

  node*
  delete_tree();

  void
  delete_tree_recursive( node* T );
};

template<typename point>
using NODE = typename ParallelKDtree<point>::node;
