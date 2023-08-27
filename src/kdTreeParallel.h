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
    size_t aug;
    node* parent;
  };

  struct leaf : node {
    points pts;
    leaf( slice In ) : node{ true, static_cast<size_t>( In.size() ), 0, nullptr } {
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
    interior( node* _left, node* _right, splitter _split ) :
        node{ false, _left->size + _right->size, 0, nullptr },
        left( _left ),
        right( _right ),
        split( _split ) {
      left->parent = this;
      right->parent = this;
    }
  };

  using node_box = std::pair<node*, box>;
  enum split_rule { MAX_STRETCH_DIM, ROTATE_DIM };

 private:
  node* root = nullptr;
  parlay::internal::timer timer;
  // split_rule _split_rule = ROTATE_DIM;
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
  static constexpr uint_fast8_t INBALANCE_RATIO = 30;

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

    //* the node which needs to be rebuilt has tag BUCKET_NUM+3
    //* the node whose ancestor has been rebuilt has tag BUCKET_NUM+2
    //* otherwise it has tag BUCKET_NUM+1
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

 public:
  inline void
  set_root( node* _root ) {
    this->root = _root;
  }

  inline node*
  get_root() {
    return this->root;
  }

  inline box
  get_box() {
    return this->bbox;
  }

  //@ Support Functions
  parlay::type_allocator<leaf> leaf_allocator;
  parlay::type_allocator<interior> interior_allocator;

  static leaf*
  alloc_leaf_node( slice In ) {
    leaf* o = parlay::type_allocator<leaf>::alloc();
    new ( o ) leaf( In );
    return o;
  }

  static interior*
  alloc_interior_node( node* L, node* R, const splitter& split ) {
    interior* o = parlay::type_allocator<interior>::alloc();
    new ( o ) interior( L, R, split );
    return o;
  }

  static void
  free_leaf( node* T ) {
    parlay::type_allocator<leaf>::retire( static_cast<leaf*>( T ) );
  }

  static void
  free_interior( node* T ) {
    parlay::type_allocator<interior>::retire( static_cast<interior*>( T ) );
  }

  static inline bool
  inbalance_node( const size_t& l, const size_t& n ) {
    if ( n == 0 ) return true;
    return Gt( std::abs( 100.0 * l / n - 50.0 ), 1.0 * INBALANCE_RATIO );
  }

  static inline bool
  legal_box( const box& bx ) {
    if ( bx == get_empty_box() ) return true;
    for ( int i = 0; i < bx.first.get_dim(); i++ ) {
      if ( bx.first.pnt[i] > bx.second.pnt[i] ) {
        return false;
      }
    }
    return true;
  }

  static inline bool
  within_box( const box& a, const box& b ) {
    if ( !legal_box( a ) ) {
      LOG << a.first << " " << a.second << ENDL;
    }
    assert( legal_box( a ) );
    assert( legal_box( b ) );
    for ( int i = 0; i < a.first.get_dim(); i++ ) {
      if ( a.first.pnt[i] < b.first.pnt[i] || a.second.pnt[i] > b.second.pnt[i] ) {
        return false;
      }
    }
    return true;
  }

  static inline bool
  within_box( const point& p, const box& bx ) {
    assert( legal_box( bx ) );
    for ( int i = 0; i < p.get_dim(); i++ ) {
      if ( p.pnt[i] < bx.first.pnt[i] || p.pnt[i] > bx.second.pnt[i] ) {
        return false;
      }
    }
    return true;
  }

  static inline bool
  intersect_box( const box& a, const box& b ) {
    assert( legal_box( a ) && legal_box( b ) );
    for ( int i = 0; i < a.first.get_dim(); i++ ) {
      if ( a.first.pnt[i] > b.second.pnt[i] ) {
        return false;
      }
    }
    return true;
  }

  static inline box
  get_empty_box() {
    return std::move( box( point( std::numeric_limits<coord>::max() ),
                           point( std::numeric_limits<coord>::min() ) ) );
  }

  static box
  get_box( const box& x, const box& y ) {
    return std::move(
        box( x.first.minCoords( y.first ), x.second.maxCoords( y.second ) ) );
  }

  static box
  get_box( slice V ) {
    if ( V.size() == 0 ) {
      return std::move( get_empty_box() );
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

  static box
  get_box( node* T ) {
    points wx = points::uninitialized( T->size );
    flatten( T, parlay::make_slice( wx ) );
    return std::move( get_box( parlay::make_slice( wx ) ) );
  }

  inline uint_fast8_t
  pick_rebuild_dim( const node* T, const uint_fast8_t& DIM ) {
    if ( this->_split_rule == MAX_STRETCH_DIM ) {
      return 0;
    } else if ( this->_split_rule == ROTATE_DIM ) {
      if ( T == this->root ) {
        return 0;
      } else {
        assert( !( T->parent->is_leaf ) );
        return ( static_cast<interior*>( T->parent )->split.second + 1 ) % DIM;
      }
    }
  }

  static inline uint_fast8_t
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

  static inline bool
  point_within_box_d( const point& p, const box& b, const coord& d ) {
    for ( int i = 0; i < p.get_dim(); i++ ) {
      assert( p.pnt[i] - b.first.pnt[i] >= 0 && p.pnt[i] - b.second.pnt[i] <= 0 );
      if ( p.pnt[i] - b.first.pnt[i] < d || b.second.pnt[i] - p.pnt[i] < d ) return false;
    }
    return true;
  }

  //@ Parallel KD tree cores
  //@ build
  void
  divide_rotate( slice In, splitter_s& pivots, uint_fast8_t dim, int idx, int deep,
                 int& bucket, const uint_fast8_t& DIM, box_s& boxs, const box& bx );

  void
  pick_pivots( slice In, const size_t& n, splitter_s& pivots, const uint_fast8_t& dim,
               const uint_fast8_t& DIM, box_s& boxs, const box& bx );

  static inline uint_fast32_t
  find_bucket( const point& p, const splitter_s& pivots );

  static void
  partition( slice A, slice B, const size_t& n, const splitter_s& pivots,
             parlay::sequence<uint_fast32_t>& sums );

  static node*
  build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                    parlay::sequence<node*>& treeNodes );

  void
  build( slice In, const uint_fast8_t& DIM );

  node*
  build_recursive( slice In, slice Out, uint_fast8_t dim, const uint_fast8_t& DIM,
                   box bx );

  //@ batch insert
  void
  batchInsert( slice In, const uint_fast8_t& DIM );

  static void
  flatten( node* T, slice Out );

  void
  flatten_and_delete( node* T, slice Out );

  static inline void
  update_interior( node* T, node* L, node* R );

  static void
  seieve_points( slice A, slice B, const size_t& n, const node_tags& tags,
                 parlay::sequence<uint_fast32_t>& sums, const int& tagsNum );

  static inline uint_fast32_t
  retrive_tag( const point& p, const node_tags& tags );

  static node*
  update_inner_tree( uint_fast32_t idx, const node_tags& tags,
                     parlay::sequence<node*>& treeNodes, int& p,
                     const tag_nodes& rev_tag );

  node*
  batchInsert_recusive( node* T, slice In, slice Out, const uint_fast8_t& DIM );

  void
  batchDelete( slice In, const uint_fast8_t& DIM );

  node_box
  batchDelete_recursive( node* T, slice In, slice Out, const uint_fast8_t& DIM,
                         bool hasTomb );

  node_box
  delete_inner_tree( uint_fast32_t idx, const node_tags& tags,
                     parlay::sequence<node_box>& treeNodes, int& p,
                     const tag_nodes& rev_tag, const uint_fast8_t& DIM );

  node*
  delete_tree();

  static void
  delete_tree_recursive( node* T );

  //@ query stuffs
  static inline coord
  ppDistanceSquared( const point& p, const point& q, const uint_fast8_t& DIM );

  static void
  k_nearest( node* T, const point& q, const uint_fast8_t& DIM, kBoundedQueue<coord>& bq,
             size_t& visNodeNum );

  size_t
  range_count( const box& queryBox );

  static size_t
  range_count_value( node* T, const box& queryBox, const box& nodeBox );

  //* cannot run in parallel
  static void
  range_count_recursive( node* T, const box& queryBox, const box& nodeBox );

  // TODO add more Out type
  size_t
  range_query( const box& queryBox, slice Out );

  static void
  range_query_recursive( node* T, slice In, size_t& s, const box& queryBox,
                         const box& nodeBox );

  //@ validations
  static bool
  checkBox( node* T, const box& bx ) {
    assert( T != nullptr );
    assert( legal_box( bx ) );
    points wx = points::uninitialized( T->size );
    flatten( T, parlay::make_slice( wx ) );
    auto b = get_box( parlay::make_slice( wx ) );
    return within_box( get_box( parlay::make_slice( wx ) ), bx );
  }

  static size_t
  checkSize( node* T ) {
    if ( T->is_leaf ) {
      return T->size;
    }
    interior* TI = static_cast<interior*>( T );
    size_t l = checkSize( TI->left );
    size_t r = checkSize( TI->right );
    assert( l + r == T->size );
    return T->size;
  }

  void
  checkTreeSameSequential( node* T, int dim, const int& DIM ) {
    if ( T->is_leaf ) {
      assert( pick_rebuild_dim( T, DIM ) == dim );
      return;
    }
    interior* TI = static_cast<interior*>( T );
    assert( TI->split.second == dim );
    dim = ( dim + 1 ) % DIM;
    parlay::par_do_if(
        T->size > 1000, [&]() { checkTreeSameSequential( TI->left, dim, DIM ); },
        [&]() { checkTreeSameSequential( TI->right, dim, DIM ); } );
    return;
  }

  void
  validate( const uint_fast8_t& DIM ) {
    if ( checkBox( this->root, this->bbox ) && legal_box( this->bbox ) ) {
      std::cout << "Correct bounding box" << std::endl << std::flush;
    } else {
      std::cout << "wrong bounding box" << std::endl << std::flush;
      abort();
    }

    if ( this->_split_rule == ROTATE_DIM ) {
      checkTreeSameSequential( this->root, 0, DIM );
      std::cout << "Correct rotate dimension" << std::endl << std::flush;
    }

    if ( checkSize( this->root ) == this->root->size ) {
      std::cout << "Correct size" << std::endl << std::flush;
    } else {
      std::cout << "wrong tree size" << std::endl << std::flush;
      abort();
    }
    return;
  }
};

template<typename point>
using NODE = typename ParallelKDtree<point>::node;

template<typename point>
using BOX = typename ParallelKDtree<point>::box;
