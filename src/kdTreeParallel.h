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

  struct node {
    bool is_leaf;
    size_t size;
    uint_fast8_t dim;
  };

  struct leaf : node {
    points pts;
    leaf( slice In, const uint_fast8_t& dim ) :
        node{ true, static_cast<size_t>( In.size() ), dim } {
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
        node{ false, _left->size + _right->size, dim },
        left( _left ),
        right( _right ),
        split( _split ) {}
  };

 private:
  node* root = nullptr;
  parlay::internal::timer timer;

 public:
  size_t rebuild_time = 0;

 public:
  //@ tag each node an bucket id
  using node_tag = std::pair<node*, int_fast32_t>;
  using node_tags = parlay::sequence<node_tag>;
  using tag_nodes = parlay::sequence<uint_fast32_t>;  //*index by tag

  //@ Const variables
  //@ uint32t handle up to 4e9 at least
  static constexpr uint_fast32_t BUILD_DEPTH_ONCE = 5;  //* last layer is leaf
  static constexpr uint_fast32_t PIVOT_NUM = ( 1 << BUILD_DEPTH_ONCE ) - 1;
  static constexpr uint_fast32_t BUCKET_NUM = 1 << BUILD_DEPTH_ONCE;
  //@ general
  static constexpr uint_fast32_t LEAVE_WRAP = 32;
  static constexpr uint_fast32_t SERIAL_BUILD_CUTOFF = 1 << 10;
  //@ block param in partition
  static constexpr uint_fast32_t LOG2_BASE = 10;
  static constexpr uint_fast32_t BLOCK_SIZE = 1 << LOG2_BASE;
  //@ reconstruct ratio
  static constexpr uint_fast8_t INBALANCE_RATIO = 20;

  using box = std::pair<point, point>;

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

    inline int
    get_node_idx( int idx, node* T ) {
      if ( tags[idx].first == T ) return idx;
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return -1;
      auto pos = get_node_idx( idx << 1, T );
      if ( pos != -1 ) return pos;
      return get_node_idx( idx << 1 | 1, T );
    }

    void
    add_sum_to_node( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        tags[idx].first->size += sums[tags[idx].second];
        return;
      }
      add_sum_to_node( idx << 1 );
      add_sum_to_node( idx << 1 | 1 );
      tags[idx].first->size = tags[idx << 1].first->size + tags[idx << 1 | 1].first->size;
      return;
    }

    void
    decrease_sum_from_node( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        assert( tags[idx].second < BUCKET_NUM );
        tags[idx].first->size -= sums[std::abs( tags[idx].second )];
        return;
      }
      decrease_sum_from_node( idx << 1 );
      decrease_sum_from_node( idx << 1 | 1 );
      tags[idx].first->size = tags[idx << 1].first->size + tags[idx << 1 | 1].first->size;
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
      if ( Gt( std::abs( 100.0 * ( TI->left->size + sums_tree[idx << 1] ) /
                             ( TI->size + sums_tree[idx] ) -
                         50 ),
               INBALANCE_RATIO * 1.0 ) ) {
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

    //@ variables
    node_tags tags = node_tags::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
    parlay::sequence<uint_fast32_t> sums;
    mutable parlay::sequence<uint_fast32_t> sums_tree =
        parlay::sequence<uint_fast32_t>( PIVOT_NUM + BUCKET_NUM + 1 );
    mutable tag_nodes rev_tag = tag_nodes::uninitialized( BUCKET_NUM );
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

  //@ Parallel KD tree cores
  //@ build
  void
  divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim, int idx, int deep,
                 int& bucket, const uint_fast8_t& DIM );

  void
  pick_pivots( slice In, const size_t& n, splitter_s& pivots, const uint_fast8_t& dim,
               const uint_fast8_t& DIM );

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
  build_recursive( slice In, slice Out, uint_fast8_t dim, const uint_fast8_t& DIM );

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

  node*
  delete_tree();
  void
  delete_tree_recursive( node* T );
};

template<typename point>
using NODE = typename ParallelKDtree<point>::node;
