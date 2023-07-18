#pragma once

#include "utility.h"

#define LOG std::cout
#define ENDL std::endl

template <typename point>
class ParallelKDtree {
 public:
   using points = parlay::sequence<point>;
   using coords = typename point::coords;
   //@ take the value of a point in specific dimension
   using splitter = std::pair<coord, uint_fast32_t>;
   using splitter_s = parlay::sequence<splitter>;
   using slice = parlay::slice<point*, point*>;
   //@ Const variables
   //@ uint32t handle up to 4e9 at least
   static constexpr uint_fast32_t BUILD_DEPTH_ONCE = 5; //* last layer is leaf
   static constexpr uint_fast32_t PIVOT_NUM =
       ( 1 << BUILD_DEPTH_ONCE ) - 1; //* 2^i -1
   static constexpr uint_fast32_t BUCKET_NUM = 1 << BUILD_DEPTH_ONCE;
   //@ general
   static constexpr uint_fast32_t LEAVE_WRAP = 32;
   static constexpr uint_fast32_t SERIAL_BUILD_CUTOFF = 1 << 10;
   //@ block param in partition
   static constexpr uint_fast32_t LOG2_BASE = 10;
   static constexpr uint_fast32_t BLOCK_SIZE = 1 << LOG2_BASE;
   using box = std::pair<coords, coords>;

 public:
   struct pointLess {
      pointLess( size_t index ) : index_( index ) {}
      bool
      operator()( const point& p1, const point& p2 ) const {
         return p1.pnt[index_] < p2.pnt[index_];
      }
      size_t index_;
   };

 private:
   int Dims;

   // TODO: handle double precision

   // Given two points, return the min. value on each dimension
   // minv[i] = smaller value of two points on i-th dimension

   // **************************************************************
   // Tree structure, leafs and interior extend the base node class
   // **************************************************************
 public:
   struct node {
      bool is_leaf;
      size_t size;
   };

   struct leaf : node {
      points pts;
      leaf( points pts )
          : node{ true, static_cast<size_t>( pts.size() ) }, pts( pts ) {}
   };

   // todo replace split and cut_dim by splitter
   struct interior : node {
      node* left;
      node* right;
      splitter split;
      interior( node* _left, node* _right, splitter _split )
          : node{ false, _left->size + _right->size }, left( _left ),
            right( _right ), split( _split ) {}
   };

   // struct node {
   //    bool is_leaf;
   //    size_t size;
   //    box bounds;
   //    node* parent;
   // };
   // struct leaf : node {
   //    points pts;
   //    leaf( points pts )
   //        : node{ true, static_cast<size_t>( pts.size() ),
   //                bound_box_from_points( pts ), nullptr },
   //          pts( pts ) {}
   // };
   // // todo replace split and cut_dim by splitter
   // struct interior : node {
   //    node* left;
   //    node* right;
   //    splitter split;
   //    interior( node* _left, node* _right, splitter _split )
   //        : node{ false, _left->size + _right->size,
   //                bound_box_from_boxes( _left->bounds, _right->bounds ),
   //                nullptr },
   //          left( _left ), right( _right ), split( _split ) {
   //       left->parent = this;
   //       right->parent = this;
   //    }
   // };

   //@ Support Functions
   parlay::type_allocator<leaf> leaf_allocator;
   parlay::type_allocator<interior> interior_allocator;

   void
   divide_rotate( slice In, splitter_s& pivots, int dim, int idx, int deep,
                  int& bucket, const int& DIM );

   template <typename slice>
   std::array<uint_fast32_t, PIVOT_NUM>
   partition( slice A, slice B, const size_t& n,
              const std::array<coord, PIVOT_NUM>& pivots, const int& dim );

   void
   pick_pivots( slice In, const size_t& n, splitter_s& pivots, const int& dim,
                const int& DIM );

   inline uint_fast32_t
   find_bucket( const point& p, const splitter_s& pivots, const int& dim,
                const int& DIM );

   void
   partition( slice A, slice B, const size_t& n, const splitter_s& pivots,
              parlay::sequence<uint_fast32_t>& sums, const int& dim,
              const int& DIM );

   node*
   build_inner_tree( uint_fast16_t idx, splitter_s& pivots,
                     parlay::sequence<node*>& treeNodes );

   inline coord
   ppDistanceSquared( const point& p, const point& q, const int& DIM );

   //@ Parallel KD tree cores
   node*
   build( slice In, slice Out, int dim, const int& DIM );

   void
   k_nearest( node* T, const point& q, const int& DIM, kBoundedQueue<coord>& bq,
              size_t& visNodeNum );

   void
   delete_tree( node* T );
};
