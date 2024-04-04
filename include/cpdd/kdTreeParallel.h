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
    using points_iter = typename parlay::sequence<point>::iterator;
    using splitter = std::pair<coord, dim_type>;
    using splitter_s = parlay::sequence<splitter>;
    using box = std::pair<point, point>;
    using box_s = parlay::sequence<box>;
    using circle = std::pair<point, coord>;

    //@ Const variables
    //@ uint32t handle up to 4e9 at least
    //! bucket num should smaller than 1<<8 to handle type overflow

    // TODO: wrap the variables using std::getenv()
    static constexpr bucket_type BUILD_DEPTH_ONCE = 6;  //* last layer is leaf
    static constexpr bucket_type PIVOT_NUM = (1 << BUILD_DEPTH_ONCE) - 1;
    static constexpr bucket_type BUCKET_NUM = 1 << BUILD_DEPTH_ONCE;
    // NOTE: tree structure
    static constexpr uint_fast8_t LEAVE_WRAP = 32;
    static constexpr uint_fast8_t THIN_LEAVE_WRAP = 24;
    static constexpr uint_fast16_t SERIAL_BUILD_CUTOFF = 1 << 10;
    // NOTE: block param in partition
    static constexpr uint_fast8_t LOG2_BASE = 10;
    static constexpr uint_fast16_t BLOCK_SIZE = 1 << LOG2_BASE;
    // NOTE: reconstruct weight threshold
    static constexpr uint_fast8_t INBALANCE_RATIO = 30;
    // NOTE: tag indicates whether poitns are full covered in the tree
    struct FullCoveredTag {};
    struct PartialCoverTag {};

    //*------------------- Tree Structures--------------------*//
    //@ kd tree node types and functions
    struct node;
    struct leaf;
    struct interior;
    //@ range query tree node
    struct simple_node;

    static leaf* alloc_leaf_node(slice In);
    static leaf* alloc_dummy_leaf(slice In);
    static leaf* alloc_empty_leaf();
    static interior* alloc_interior_node(node* L, node* R, const splitter& split);
    static simple_node* alloc_simple_node(simple_node* L, simple_node* R);
    static simple_node* alloc_simple_node(size_t sz);
    static void free_leaf(node* T);
    static void free_interior(node* T);
    static void free_simple_node(simple_node* T);
    static inline size_t get_imbalance_ratio();
    static inline bool inbalance_node(const size_t l, const size_t n);

    using node_box = std::pair<node*, box>;
    using node_tag = std::pair<node*, uint_fast8_t>;
    using node_tags = parlay::sequence<node_tag>;
    using tag_nodes = parlay::sequence<balls_type>;  //*index by tag

    enum split_rule { MAX_STRETCH_DIM, ROTATE_DIM };

    //@ array based inner tree for batch insertion and deletion
    struct InnerTree;

    //@ box operations
    static inline bool legal_box(const box& bx);
    static inline bool within_box(const box& a, const box& b);
    static inline bool within_box(const point& p, const box& bx);
    static inline bool box_intersect_box(const box& a, const box& b);
    static inline box get_empty_box();
    static box get_box(const box& x, const box& y);
    static box get_box(slice V);
    static box get_box(node* T);

    static inline bool within_circle(const box& bx, const circle& cl);
    static inline bool within_circle(const point& p, const circle& cl);
    static inline bool circle_intersect_box(const circle& cl, const box& bx);

    //@ dimensionality
    inline dim_type pick_rebuild_dim(const node* T, const dim_type d, const dim_type DIM);
    static inline dim_type pick_max_stretch_dim(const box& bx, const dim_type DIM);

    //@ Parallel KD tree cores
    //@ build
    void divide_rotate(slice In, splitter_s& pivots, dim_type dim, bucket_type idx, bucket_type deep,
                       bucket_type& bucket, const dim_type DIM, box_s& boxs, const box& bx);
    void pick_pivots(slice In, const size_t& n, splitter_s& pivots, const dim_type dim, const dim_type DIM, box_s& boxs,
                     const box& bx);
    static inline bucket_type find_bucket(const point& p, const splitter_s& pivots);
    static void partition(slice A, slice B, const size_t n, const splitter_s& pivots,
                          parlay::sequence<balls_type>& sums);
    static node* build_inner_tree(bucket_type idx, splitter_s& pivots, parlay::sequence<node*>& treeNodes);
    void build(slice In, const dim_type DIM);
    points_iter serial_partition(slice In, dim_type d);
    node* serial_build_recursive(slice In, slice Out, dim_type dim, const dim_type DIM, const box& bx);
    node* build_recursive(slice In, slice Out, dim_type dim, const dim_type DIM, const box& bx);

    //@ random support
    static inline uint64_t _hash64(uint64_t u);

    //@ batch helpers:
    template<typename Slice>
    static void flatten(node* T, Slice Out, bool granularity = true);

    void flatten_and_delete(node* T, slice Out);

    static void seieve_points(slice A, slice B, const size_t n, const node_tags& tags,
                              parlay::sequence<balls_type>& sums, const bucket_type tagsNum);

    static inline bucket_type retrive_tag(const point& p, const node_tags& tags);

    static node_box update_inner_tree(bucket_type idx, const node_tags& tags, parlay::sequence<node_box>& treeNodes,
                                      bucket_type& p, const tag_nodes& rev_tag);

    node_box rebuild_single_tree(node* T, const dim_type d, const dim_type DIM, const bool granularity = true);

    node_box rebuild_tree_recursive(node* T, dim_type d, const dim_type DIM, const bool granularity = true);

    node* delete_tree();

    static void delete_tree_recursive(node* T, bool granularity = true);

    static void delete_simple_tree_recursive(simple_node* T);

    //@ batch insert
    node* rebuild_with_insert(node* T, slice In, const dim_type d, const dim_type DIM);

    static inline void update_interior(node* T, node* L, node* R);

    void batchInsert(slice In, const dim_type DIM);

    node* batchInsert_recusive(node* T, slice In, slice Out, dim_type d, const dim_type DIM);

    static node* update_inner_tree_by_tag(bucket_type idx, const node_tags& tags, parlay::sequence<node*>& treeNodes,
                                          bucket_type& p, const tag_nodes& rev_tag);
    //@ batch delete

    // NOTE: in default, all points to be deleted are assumed in the tree
    void batchDelete(slice In, const dim_type DIM);

    // NOTE: explicitly specify all points to be deleted are in the tree
    void batchDelete(slice In, const dim_type DIM, FullCoveredTag);

    // NOTE: for the case that some points to be deleted are not in the tree
    void batchDelete(slice In, const dim_type DIM, PartialCoverTag);

    //  PERF: try pass a reference to bx
    node_box batchDelete_recursive(node* T, const box& bx, slice In, slice Out, dim_type d, const dim_type DIM,
                                   bool hasTomb, FullCoveredTag);

    // TODO: add bounding box for batch delete recursive as well
    // WARN: fix the possible in partial deletion as well
    node_box batchDelete_recursive(node* T, const box& bx, slice In, slice Out, dim_type d, const dim_type DIM,
                                   PartialCoverTag);

    node_box delete_inner_tree(bucket_type idx, const node_tags& tags, parlay::sequence<node_box>& treeNodes,
                               bucket_type& p, const tag_nodes& rev_tag, const dim_type d, const dim_type DIM);

    //@ query stuffs

    static inline coord p2p_distance(const point& p, const point& q, const dim_type DIM);
    static inline coord p2b_min_distance(const point& p, const box& a, const dim_type DIM);
    static inline coord p2b_max_distance(const point& p, const box& a, const dim_type DIM);
    static inline coord interruptible_distance(const point& p, const point& q, coord up, dim_type DIM);
    template<typename StoreType>
    static void k_nearest(node* T, const point& q, const dim_type DIM, kBoundedQueue<point, StoreType>& bq,
                          size_t& visNodeNum);
    template<typename StoreType>
    static void k_nearest(node* T, const point& q, const dim_type DIM, kBoundedQueue<point, StoreType>& bq,
                          const box& bx, size_t& visNodeNum);

    size_t range_count(const box& queryBox, size_t& visLeafNum, size_t& visInterNum);
    size_t range_count(const circle& cl);
    static size_t range_count_rectangle(node* T, const box& queryBox, const box& nodeBox, size_t& visLeafNum,
                                        size_t& visInterNum);
    static size_t range_count_radius(node* T, const circle& cl, const box& nodeBox);
    simple_node* range_count_save_path(node* T, const box& queryBox, const box& nodeBox);

    template<typename StoreType>
    size_t range_query_serial(const box& queryBox, StoreType Out);
    template<typename StoreType>
    size_t range_query_parallel(const typename ParallelKDtree<point>::box& queryBox, StoreType Out, double& tim);
    template<typename StoreType>
    static void range_query_recursive_serial(node* T, StoreType Out, size_t& s, const box& queryBox,
                                             const box& nodeBox);
    template<typename StoreType>
    static void range_query_recursive_parallel(node* T, simple_node* ST, StoreType Out, const box& queryBox);

    //@ validations
    static bool checkBox(node* T, const box& bx);

    static size_t checkSize(node* T);

    void checkTreeSameSequential(node* T, int dim, const int& DIM);

    void validate(const dim_type DIM);

    size_t getTreeHeight();

    size_t getMaxTreeDepth(node* T, size_t deep);

    double getAveTreeHeight();

    size_t countTreeNodesNum(node* T);

    void countTreeHeights(node* T, size_t deep, size_t& idx, parlay::sequence<size_t>& heights);

    //@ kdtree interfaces
    inline void set_root(node* _root) { this->root = _root; }

    inline node* get_root() { return this->root; }

    inline box get_root_box() { return this->bbox; }

   private:
    node* root = nullptr;
    parlay::internal::timer timer;
    // split_rule _split_rule = ROTATE_DIM;
    split_rule _split_rule = MAX_STRETCH_DIM;
    box bbox;
};

}  // namespace cpdd
