#pragma once

#include "utility.h"

#define LOG std::cout
#define ENDL std::endl

constexpr int dims = 10; // works for any constant dimension
using idx = size_t;      // index of point (int can handle up to 2^31 points)
using coord = long;      // type of each coordinate
using coords = std::array<coord, dims>; // a coord array with length dims
struct point {
   coords pnt;
};
struct pointLess {
   pointLess( size_t index ) : index_( index ) {}
   bool
   operator()( const point& p1, const point& p2 ) const {
      return p1.pnt[index_] < p2.pnt[index_];
   }
   size_t index_;
};

using points = parlay::sequence<point>;

//@ Const variables
constexpr size_t LEAVE_WRAP = 16;
constexpr size_t BUILD_DEPTH_ONCE = 16;
constexpr size_t PIVOT_NUM = BUILD_DEPTH_ONCE;
// constexpr size_t PIVOT_NUM =
//     BUILD_DEPTH_ONCE % 2 == 1 ? BUILD_DEPTH_ONCE : BUILD_DEPTH_ONCE | 1;
constexpr size_t SERIAL_BUILD_CUTOFF = 1 << 14;
constexpr size_t FOR_BLOCK_SIZE = 1 << 9;
//@ block param in partition
constexpr int log2_base = 9;
constexpr int BLOCK_SIZE = 1 << log2_base;

// **************************************************************
//! bounding box (min value on each dimension, and max on each)
// **************************************************************
using box = std::pair<coords, coords>;

// TODO: handle double precision

// Given two points, return the min. value on each dimension
// minv[i] = smaller value of two points on i-th dimension
auto minv = []( coords a, coords b ) {
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = std::min( a[i], b[i] );
   return r;
};

auto maxv = []( coords a, coords b ) {
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = std::max( a[i], b[i] );
   return r;
};

coords
center( box& b );

box
bound_box( const parlay::sequence<point>& P );

box
bound_box( const box& b1, const box& b2 );

// **************************************************************
// Tree structure, leafs and interior extend the base node class
// **************************************************************
struct node {
   bool is_leaf;
   idx size;
   box bounds;
   node* parent;
};

struct leaf : node {
   points pts;
   leaf( points pts )
       : node{ true, static_cast<idx>( pts.size() ), bound_box( pts ),
               nullptr },
         pts( pts ) {}
};

struct interior : node {
   node* left;
   node* right;
   coord split;
   int cut_dim;
   interior( node* _left, node* _right, coord _split, int _cut_dim )
       : node{ false, _left->size + _right->size,
               bound_box( _left->bounds, _right->bounds ), nullptr },
         left( _left ), right( _right ), split( _split ), cut_dim( _cut_dim ) {
      left->parent = this;
      right->parent = this;
   }
};

//@ Support Functions
parlay::type_allocator<leaf>;
parlay::type_allocator<interior>;

template <typename slice>
std::array<coord, PIVOT_NUM>
pick_pivots( slice A, const size_t& n, const int& dim );

template <typename slice>
std::array<int, PIVOT_NUM>
partition( slice A, slice B, const size_t& n,
           const std::array<coord, PIVOT_NUM>& pivots, const int& dim );

template <typename slice>
coord
pick_single_pivot( slice A, const size_t& n, const int& dim );

coord
ppDistanceSquared( const point& p, const point& q, int DIM );

//@ Parallel KD tree cores
template <typename slice>
node*
build( slice In, slice Out, int dim, const int& DIM,
       std::array<coord, PIVOT_NUM> pivots, int pn,
       std::array<int, PIVOT_NUM> sums );

void
k_nearest( node* T, const point& q, const int DIM, kBoundedQueue<coord>& bq );

void
delete_tree( node* T );