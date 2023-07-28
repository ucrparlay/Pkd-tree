#pragma once

#include "utility.h"

#define LOG  std::cout
#define ENDL std::endl

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
    //@ tag each node an bucket id
    using node_tag = std::pair<node*, uint_fast32_t>;
    using node_tags = parlay::sequence<node_tag>;
    using tag_nodes = parlay::sequence<node*>;  //*index by tag

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
    using box = std::pair<point, point>;

   public:
    struct pointLess {
        pointLess( size_t index ) : index_( index ) {}
        bool operator()( const point& p1, const point& p2 ) const {
            return p1.pnt[index_] < p2.pnt[index_];
        }
        size_t index_;
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

    //@ Parallel KD tree cores
    void build( slice In, const uint_fast8_t& DIM );
    node* build_recursive( slice In, slice Out, uint_fast8_t dim,
                           const uint_fast8_t& DIM );

    void k_nearest( node* T, const point& q, const uint_fast8_t& DIM,
                    kBoundedQueue<coord>& bq, size_t& visNodeNum );

    void batchInsert( slice In, const uint_fast8_t& DIM );
    inline void update_interior( node* T, node* L, node* R );
    void assign_node_tag( node_tags& tags, tag_nodes& tnodes, node* T, uint_fast8_t dim,
                          const uint_fast8_t& DIM, int deep, int idx, int& bucket );
    void seieve_points( slice A, slice B, const size_t& n, const node_tags& tags,
                        parlay::sequence<uint_fast32_t>& sums, const uint_fast8_t& dim,
                        const uint_fast8_t& DIM, const int& tagsNum );
    inline uint_fast32_t retrive_tag( const point& p, const node_tags& tags,
                                      const uint_fast8_t& dim, const uint_fast8_t& DIM );
    inline node* retrive_node( const uint_fast32_t& id, const node_tags& tags,
                               uint_fast8_t dim, uint_fast32_t idx,
                               const uint_fast8_t& DIM );
    node* update_inner_tree( uint_fast32_t idx, const node_tags& tags,
                             parlay::sequence<node*>& treeNodes );
    node* batchInsert_recusive( node* T, slice In, slice Out, uint_fast8_t dim,
                                const uint_fast8_t& DIM );

    node* delete_tree();
    void delete_tree_recursive( node* T );
};

template<typename point>
using NODE = typename ParallelKDtree<point>::node;
