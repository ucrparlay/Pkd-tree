#pragma once

#include "utility.h"

constexpr int dims = 15; // works for any constant dimension
using idx = int;         // index of point (int can handle up to 2^31 points)
using coord = double;    // type of each coordinate
using coords = std::array<coord, dims>;
struct point
{
   idx id;
   coords pnt;
};
struct pointComparator : point
{
   pointComparator( size_t index ) : index_( index ) {}
   bool
   operator()( const point& p1, const point& p2 ) const
   {
      return p1.pnt[index_] < p2.pnt[index_];
   }
   size_t index_;
};

using points = parlay::sequence<point>;

// max leaf size of tree
constexpr int node_size_cutoff = 16;

// **************************************************************
// bounding box (min value on each dimension, and max on each)
// **************************************************************
using box = std::pair<coords, coords>;

// TODO: handle double precision

// Given two points, return the min. value on each dimension
// minv[i] = smaller value of two points on i-th dimension
auto minv = []( coords a, coords b )
{
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = std::min( a[i], b[i] );
   return r;
};

auto maxv = []( coords a, coords b )
{
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = std::max( a[i], b[i] );
   return r;
};

coords
center( box& b )
{
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = ( b.first[i] + b.second[i] ) / 2;
   return r;
}

box
bound_box( const parlay::sequence<point>& P )
{
   auto pts = parlay::map( P, []( point p ) { return p.pnt; } );
   auto x = box{ parlay::reduce( pts, parlay::binary_op( minv, coords() ) ),
                 parlay::reduce( pts, parlay::binary_op( maxv, coords() ) ) };
   return x;
}

box
bound_box( const box& b1, const box& b2 )
{
   return box{ minv( b1.first, b2.first ), maxv( b1.second, b2.second ) };
}

// **************************************************************
// Tree structure, leafs and interior extend the base node class
// **************************************************************
struct node
{
   bool is_leaf;
   idx size;
   box bounds;
   node* parent;
};

struct leaf : node
{
   points pts;
   leaf( points pts )
       : node{ true, static_cast<idx>( pts.size() ), bound_box( pts ),
               nullptr },
         pts( pts )
   {
   }
};

struct interior : node
{
   node* left;
   node* right;
   interior( node* left, node* right )
       : node{ false, left->size + right->size,
               bound_box( left->bounds, right->bounds ), nullptr },
         left( left ), right( right )
   {
      left->parent = this;
      right->parent = this;
   }
};

parlay::type_allocator<leaf> leaf_allocator;
parlay::type_allocator<interior> interior_allocator;

template <typename slice>
node*
build( slice P, int i, int DIM )
{
   int n = P.size();
   if( n <= node_size_cutoff )
   {
      return leaf_allocator.allocate( parlay::to_sequence( P ) );
   }
   std::nth_element( P.begin(), P.begin() + n / 2, P.end(),
                     pointComparator( i ) );
   i = ( i + 1 ) % DIM;

   node *L, *R;
   parlay::par_do( [&]() { L = build( P.cut( 0, n / 2 ), i, DIM ); },
                   [&]() { R = build( P.cut( n / 2, n ), i, DIM ); } );
   return interior_allocator.allocate( L, R );
}

node*
kdTree( const parlay::sequence<coords>& P, long base_size = node_size_cutoff,
        int DIM = 3 )
{
   // tag points with identifiers
   points pts = parlay::tabulate( P.size(),
                                  [&]( idx i ) {
                                     return point{ i, P[i] };
                                  } );

   // build tree on top of morton ordering
   return build( pts.cut( 0, P.size() ), 0, DIM );
}

void
queryKNN( node* T, point q, int k )
{
   if( T->is_leaf )
   {
      for( int i = 0; i < T->size; i++ )
      {
         leaf* TL = static_cast<leaf*>( T );
         point p = TL->pts[i];
      }
      return;
   }

   interior* TI = static_cast<interior*>( T );
   long n_left = TI->left->size;
   long n = T->size;

   parlay::par_do( [&]() { queryKNN( TI->left, q, k ); },
                   [&]() { queryKNN( TI->right, q, k ); } );
}
