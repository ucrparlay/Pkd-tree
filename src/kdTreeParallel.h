#pragma once

#include "utility.h"

#define LOG  std::cout
#define ENDL std::endl << std::flush

template<typename point>
class ParallelKDtree {

  // **************************************************************
  // Tree structure, leafs and interior extend the base node class
  // **************************************************************

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
  node* root = NULL;
  parlay::internal::timer timer;

 public:
  size_t rebuild_time = 0;

 public:
  //@ tag each node an bucket id
  using node_tag = std::pair<node*, uint_fast32_t>;
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
    bool operator()( const point& p1, const point& p2 ) const {
      return p1.pnt[index_] < p2.pnt[index_];
    }
    size_t index_;
  };

  struct InnerTree {
    void assert_size( node* T ) {
      if ( T->is_leaf ) {
        return;
      }
      interior* TI = static_cast<interior*>( T );
      assert( TI->size == TI->left->size + TI->right->size );
      return;
    }
    void assert_size_by_idx( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return;
      interior* TI = static_cast<interior*>( tags[idx].first );
      assert( TI->size == TI->left->size + TI->right->size );
      return;
    }
    void reset_tags_num() { tagsNum = 0; }
    void assign_node_tag( node* T, uint_fast8_t dim, const uint_fast8_t& DIM, int deep,
                          int idx ) {
      if ( T->is_leaf || deep > BUILD_DEPTH_ONCE ) {
        assert( tagsNum < BUCKET_NUM );
        tags[idx] = node_tag( T, tagsNum++ );
        return;
      }
      //* BUCKET ID in [0, BUCKET_NUM)
      tags[idx] = node_tag( T, BUCKET_NUM );
      interior* TI = static_cast<interior*>( T );
      dim = ( dim + 1 ) % DIM;
      assign_node_tag( TI->left, dim, DIM, deep + 1, idx << 1 );
      assign_node_tag( TI->right, dim, DIM, deep + 1, idx << 1 | 1 );
      return;
    }

    inline int get_node_idx( int idx, node* T ) {
      if ( tags[idx].first == T ) return idx;
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) return -1;
      auto pos = get_node_idx( idx << 1, T );
      if ( pos != -1 ) return pos;
      return get_node_idx( idx << 1 | 1, T );
    }
    inline uint_fast32_t reduce_sums( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        assert( tags[idx].second < BUCKET_NUM );
        return sums[tags[idx].second];
      }
      return reduce_sums( idx << 1 ) + reduce_sums( idx << 1 | 1 );
    }
    void add_sum_to_node( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        tags[idx].first->size += sums[tags[idx].second];
        return;
      }
      add_sum_to_node( idx << 1 );
      add_sum_to_node( idx << 1 | 1 );
      tags[idx].first->size = tags[idx << 1].first->size + tags[idx << 1 | 1].first->size;
      return;
    }
    void decrease_sum_from_node( int idx ) {
      if ( idx > PIVOT_NUM || tags[idx].first->is_leaf ) {
        if ( tags[idx].first->size == 15902 || tags[idx].first->size == 15923 ) {
          LOG << "appear in decrease" << ENDL;
          abort();
        }
        tags[idx].first->size -= sums[tags[idx].second];
        if ( tags[idx].first->size == 15902 || tags[idx].first->size == 15923 ) {
          LOG << "appear in decrease" << ENDL;
          abort();
        }
        return;
      }
      decrease_sum_from_node( idx << 1 );
      decrease_sum_from_node( idx << 1 | 1 );

      tags[idx].first->size = tags[idx << 1].first->size + tags[idx << 1 | 1].first->size;
      if ( tags[idx].first->size == 79257 ) {
        LOG << "appear in decrease" << ENDL;
        abort();
      }
      return;
    }

    bool inbalance_node( node* T ) {
      assert( !T->is_leaf );
      interior* TI = static_cast<interior*>( T );
      size_t l = TI->left->size;
      size_t r = TI->right->size;
      assert( Leq( std::abs( 100.0 * l / ( l + r ) - 50 ), 50.0 ) &&
              Geq( std::abs( 100.0 * l / ( l + r ) - 50 ), 0.0 ) );
      return Gt( std::abs( 100.0 * l / ( l + r ) - 50 ), INBALANCE_RATIO * 1.0 );
    }

    void re_tag( int idx, int deep ) {
      if ( deep > BUILD_DEPTH_ONCE || tags[idx].first->is_leaf ) {
        tags[idx].second = tagsNum;
        rev_tag[tagsNum++] = idx;
        return;
      }
      interior* TI = static_cast<interior*>( tags[idx].first );
      assert( tags[idx].second == BUCKET_NUM );
      assert( TI->size == TI->left->size + TI->right->size );

      // if ( inbalance_node( tags[idx].first ) ) {
      if ( 0 ) {
        //! mark to run rebuild when parallel
        tags[idx].second = BUCKET_NUM + 1 + tagsNum;
        rev_tag[tagsNum++] = idx;
        return;
      }
      tags[idx].second = BUCKET_NUM;
      re_tag( idx << 1, deep + 1 );
      re_tag( idx << 1 | 1, deep + 1 );
      return;
    }

    void tag_inbalance_node() {
      // add_sum_to_node( 1 );
      reset_tags_num();
      re_tag( 1, 1 );
      // decrease_sum_from_node( 1 );
      assert_size_by_idx( 1 );
    }

    //@ variables
    node_tags tags = node_tags::uninitialized( PIVOT_NUM + BUCKET_NUM + 1 );
    parlay::sequence<uint_fast32_t> sums;
    tag_nodes rev_tag = tag_nodes::uninitialized( BUCKET_NUM );
    int tagsNum;
  };

  // TODO: handle double precision

  // Given two points, return the min. value on each dimension
  // minv[i] = smaller value of two points on i-th dimension

 public:
  inline void set_root( node* _root ) { this->root = _root; }
  inline node* get_root() { return this->root; }

  //@ Support Functions
  parlay::type_allocator<leaf> leaf_allocator;
  parlay::type_allocator<interior> interior_allocator;

  leaf* alloc_leaf_node( slice In, const uint_fast8_t& dim ) {
    leaf* o = parlay::type_allocator<leaf>::alloc();
    new ( o ) leaf( In, dim );
    return o;
  }

  interior* alloc_interior_node( node* L, node* R, const splitter& split,
                                 const uint_fast8_t& dim ) {
    interior* o = parlay::type_allocator<interior>::alloc();
    new ( o ) interior( L, R, split, dim );
    return o;
  }

  void free_leaf( node* T ) {
    parlay::type_allocator<leaf>::retire( static_cast<leaf*>( T ) );
  }
  void free_interior( node* T ) {
    parlay::type_allocator<interior>::retire( static_cast<interior*>( T ) );
  }

  //@ Parallel KD tree cores
  //@ build
  void divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim, int idx, int deep,
                      int& bucket, const uint_fast8_t& DIM );

  void pick_pivots( slice In, const size_t& n, splitter_s& pivots,
                    const uint_fast8_t& dim, const uint_fast8_t& DIM );

  inline uint_fast32_t find_bucket( const point& p, const splitter_s& pivots,
                                    const uint_fast8_t& dim, const uint_fast8_t& DIM );

  void partition( slice A, slice B, const size_t& n, const splitter_s& pivots,
                  parlay::sequence<uint_fast32_t>& sums, const uint_fast8_t& dim,
                  const uint_fast8_t& DIM );

  node* build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                          parlay::sequence<node*>& treeNodes );

  inline coord ppDistanceSquared( const point& p, const point& q,
                                  const uint_fast8_t& DIM );

  void build( slice In, const uint_fast8_t& DIM );
  node* build_recursive( slice In, slice Out, uint_fast8_t dim, const uint_fast8_t& DIM );

  void k_nearest( node* T, const point& q, const uint_fast8_t& DIM,
                  kBoundedQueue<coord>& bq, size_t& visNodeNum );

  //@ batch insert
  void batchInsert( slice In, const uint_fast8_t& DIM );
  void flatten( node* T, slice Out );
  inline void update_interior( node* T, node* L, node* R );
  void assign_node_tag( node_tags& tags, node* T, uint_fast8_t dim,
                        const uint_fast8_t& DIM, int deep, int idx, int& bucket );
  void seieve_points( slice A, slice B, const size_t& n, const node_tags& tags,
                      parlay::sequence<uint_fast32_t>& sums, const uint_fast8_t& dim,
                      const uint_fast8_t& DIM, const int& tagsNum, const bool seieve );
  inline uint_fast32_t retrive_tag( const point& p, const node_tags& tags,
                                    const uint_fast8_t& dim, const uint_fast8_t& DIM );
  inline node* retrive_node( const uint_fast32_t& id, const node_tags& tags,
                             uint_fast8_t dim, uint_fast32_t idx,
                             const uint_fast8_t& DIM );
  void retag_inbalance_node( node_tags& tags, const parlay::sequence<uint_fast32_t>& sums,
                             tag_nodes& tnodes, const uint_fast8_t& dim,
                             const uint_fast8_t& DIM, int& bucket );
  inline bool inbalance_node( const node* T );
  uint_fast32_t reduce_cadidates( const node_tags& tags,
                                  const parlay::sequence<uint_fast32_t>& sums,
                                  const uint_fast8_t& dim, const uint_fast8_t& DIM,
                                  node* T );
  node* update_inner_tree( uint_fast32_t idx, const node_tags& tags,
                           parlay::sequence<node*>& treeNodes );
  node* batchInsert_recusive( node* T, slice In, slice Out, uint_fast8_t dim,
                              const uint_fast8_t& DIM );

  node* delete_tree();
  void delete_tree_recursive( node* T );
};

template<typename point>
using NODE = typename ParallelKDtree<point>::node;
