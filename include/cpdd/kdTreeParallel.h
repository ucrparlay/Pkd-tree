#pragma once

#include "comparator.h"
#include "basic_point.h"
#include "query_op/nn_search_helpers.h"

namespace cpdd {

#define LOG  std::cout
#define ENDL std::endl << std::flush

template<typename point>
class ParallelKDtree {
   public:
    using bucket_type = uint_fast8_t;
    using balls_type = uint_fast32_t;
    using dim_type = uint_fast8_t;

    using coord = typename point::coord;
    using coords = typename point::coords;
    using Num = Num_Comparator<coord>;
    using slice = parlay::slice<point*, point*>;
    using points = parlay::sequence<point>;
    using points_iter = parlay::sequence<point>::iterator;
    using splitter = std::pair<coord, dim_type>;
    using splitter_s = parlay::sequence<splitter>;
    using box = std::pair<point, point>;
    using box_s = parlay::sequence<box>;

    //@ Const variables
    //@ uint32t handle up to 4e9 at least
    //! bucket num should smaller than 1<<8 to handle type overflow
    static constexpr bucket_type BUILD_DEPTH_ONCE = 6;  //* last layer is leaf
    static constexpr bucket_type PIVOT_NUM = ( 1 << BUILD_DEPTH_ONCE ) - 1;
    static constexpr bucket_type BUCKET_NUM = 1 << BUILD_DEPTH_ONCE;
    //@ tree structure
    static constexpr uint_fast8_t LEAVE_WRAP = 32;
    static constexpr uint_fast8_t THIN_LEAVE_WRAP = 24;
    static constexpr uint_fast16_t SERIAL_BUILD_CUTOFF = 1 << 10;
    //@ block param in partition
    static constexpr uint_fast8_t LOG2_BASE = 10;
    static constexpr uint_fast16_t BLOCK_SIZE = 1 << LOG2_BASE;
    //@ reconstruct weight threshold
    static constexpr uint_fast8_t INBALANCE_RATIO = 30;

    //*------------------- Tree Structures--------------------*//
    //@ tree node types and functions
    struct node;
    struct leaf;
    struct interior;

    static leaf* alloc_leaf_node( slice In );
    static leaf* alloc_dummy_leaf( slice In );
    static leaf* alloc_empty_leaf();
    static interior* alloc_interior_node( node* L, node* R, const splitter& split );
    static void free_leaf( node* T );
    static void free_interior( node* T );
    static inline bool inbalance_node( const size_t& l, const size_t& n );

    using node_box = std::pair<node*, box>;
    using node_tag = std::pair<node*, uint_fast8_t>;
    using node_tags = parlay::sequence<node_tag>;
    using tag_nodes = parlay::sequence<balls_type>;  //*index by tag

    enum split_rule { MAX_STRETCH_DIM, ROTATE_DIM };

    //@ array based inner tree for batch insertion and deletion
    struct InnerTree;

    //@ box operations
    static inline bool legal_box( const box& bx );
    static inline bool within_box( const box& a, const box& b );
    static inline bool within_box( const point& p, const box& bx );
    static inline bool intersect_box( const box& a, const box& b );
    static inline box get_empty_box();
    static box get_box( const box& x, const box& y );
    static box get_box( slice V );
    static box get_box( node* T );

    //@ dimensionality
    inline dim_type pick_rebuild_dim( const node* T, const dim_type DIM );
    static inline dim_type pick_max_stretch_dim( const box& bx, const dim_type DIM );

    //@ Parallel KD tree cores
    //@ build
    void divide_rotate( slice In, splitter_s& pivots, dim_type dim, bucket_type idx,
                        bucket_type deep, bucket_type& bucket, const dim_type DIM,
                        box_s& boxs, const box& bx );
    void pick_pivots( slice In, const size_t& n, splitter_s& pivots, const dim_type dim,
                      const dim_type DIM, box_s& boxs, const box& bx );
    static inline bucket_type find_bucket( const point& p, const splitter_s& pivots );
    static void partition( slice A, slice B, const size_t n, const splitter_s& pivots,
                           parlay::sequence<balls_type>& sums );
    static node* build_inner_tree( bucket_type idx, splitter_s& pivots,
                                   parlay::sequence<node*>& treeNodes );
    void build( slice In, const dim_type DIM );
    points_iter serial_partition( slice In, dim_type d );
    node* serial_build_recursive( slice In, slice Out, dim_type dim, const dim_type DIM,
                                  const box& bx );
    node* build_recursive( slice In, slice Out, dim_type dim, const dim_type DIM,
                           const box& bx );

    //@ batch helpers
    template<typename Slice>
    static void flatten( node* T, Slice Out );

    void flatten_and_delete( node* T, slice Out );
    static void seieve_points( slice A, slice B, const size_t n, const node_tags& tags,
                               parlay::sequence<balls_type>& sums,
                               const bucket_type tagsNum );
    static inline bucket_type retrive_tag( const point& p, const node_tags& tags );
    static node* update_inner_tree( bucket_type idx, const node_tags& tags,
                                    parlay::sequence<node*>& treeNodes, bucket_type& p,
                                    const tag_nodes& rev_tag );
    node* delete_tree();
    static void delete_tree_recursive( node* T );

    //@ batch insert
    node* rebuild_with_insert( node* T, slice In, const dim_type DIM );
    static inline void update_interior( node* T, node* L, node* R );
    void batchInsert( slice In, const dim_type DIM );
    node* serial_insert_recursive( node* T, slice In, slice Out, const dim_type DIM );
    node* batchInsert_recusive( node* T, slice In, slice Out, const dim_type DIM );

    //@ batch delete
    node_box rebuild_after_delete( node* T, const dim_type DIM );
    void batchDelete( slice In, const dim_type DIM );
    node_box batchDelete_recursive( node* T, slice In, slice Out, const dim_type DIM,
                                    bool hasTomb );
    node_box delete_inner_tree( bucket_type idx, const node_tags& tags,
                                parlay::sequence<node_box>& treeNodes, bucket_type& p,
                                const tag_nodes& rev_tag, const dim_type DIM );

    //@ query stuffs
    static inline coord ppDistanceSquared( const point& p, const point& q,
                                           const dim_type DIM );

    template<typename StoreType>
    static void k_nearest( node* T, const point& q, const dim_type DIM,
                           kBoundedQueue<point, StoreType>& bq, size_t& visNodeNum );

    size_t range_count( const box& queryBox );
    static size_t range_count_value( node* T, const box& queryBox, const box& nodeBox );

    template<typename Slice>
    size_t range_query( const box& queryBox, Slice Out );

    template<typename Slice>
    static void range_query_recursive( node* T, Slice Out, size_t& s, const box& queryBox,
                                       const box& nodeBox );

    //@ validations
    static bool checkBox( node* T, const box& bx );
    static size_t checkSize( node* T );
    void checkTreeSameSequential( node* T, int dim, const int& DIM );
    void validate( const dim_type DIM );
    int getTreeHeight( node* T, int deep );
    void countTreeHeights( node* T, int deep, int& idx, parlay::sequence<int>& heights );

    //@ kdtree interfaces
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

   private:
    node* root = nullptr;
    parlay::internal::timer timer;
    // split_rule _split_rule = ROTATE_DIM;
    split_rule _split_rule = MAX_STRETCH_DIM;
    box bbox;
};

}  // namespace cpdd