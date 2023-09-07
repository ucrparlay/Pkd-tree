#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
struct ParallelKDtree<point>::node {
  using coord = point::coord;
  using coords = point::coords;
  using points = parlay::sequence<point>;
  using slice = parlay::slice<point*, point*>;

  bool is_leaf;
  bool is_dummy;
  size_t size;
  size_t aug;
  node* parent;
};

template<typename point>
struct ParallelKDtree<point>::leaf : node {
  points pts;
  leaf( slice In ) : node{ true, false, static_cast<size_t>( In.size() ), 0, nullptr } {
    pts = points::uninitialized( LEAVE_WRAP );
    for ( int i = 0; i < In.size(); i++ ) {
      pts[i] = In[i];
    }
  }
  leaf( slice In, bool _is_dummy ) :
      node{ true, true, static_cast<size_t>( In.size() ), 0, nullptr } {
    pts = points::uninitialized( 1 );
    pts[0] = In[0];
  }
};

template<typename point>
struct ParallelKDtree<point>::interior : node {
  node* left;
  node* right;
  splitter split;
  interior( node* _left, node* _right, splitter _split ) :
      node{ false, false, _left->size + _right->size, 0, nullptr },
      left( _left ),
      right( _right ),
      split( _split ) {
    left->parent = this;
    right->parent = this;
  }
};

template<typename point>
typename ParallelKDtree<point>::leaf*
ParallelKDtree<point>::alloc_leaf_node( slice In ) {
  leaf* o = parlay::type_allocator<leaf>::alloc();
  new ( o ) leaf( In );
  assert( o->is_dummy == false );
  return std::move( o );
}

template<typename point>
typename ParallelKDtree<point>::leaf*
ParallelKDtree<point>::alloc_dummy_leaf( slice In ) {
  leaf* o = parlay::type_allocator<leaf>::alloc();
  new ( o ) leaf( In, true );
  assert( o->is_dummy == true );
  return std::move( o );
}

template<typename point>
typename ParallelKDtree<point>::interior*
ParallelKDtree<point>::alloc_interior_node( node* L, node* R, const splitter& split ) {
  interior* o = parlay::type_allocator<interior>::alloc();
  new ( o ) interior( L, R, split );
  return std::move( o );
}

template<typename point>
void
ParallelKDtree<point>::free_leaf( node* T ) {
  parlay::type_allocator<leaf>::retire( static_cast<leaf*>( T ) );
}

template<typename point>
void
ParallelKDtree<point>::free_interior( node* T ) {
  parlay::type_allocator<interior>::retire( static_cast<interior*>( T ) );
}

template<typename point>
inline bool
ParallelKDtree<point>::inbalance_node( const size_t& l, const size_t& n ) {
  if ( n == 0 ) return true;
  return Num::Gt( std::abs( 100.0 * l / n - 50.0 ), 1.0 * INBALANCE_RATIO );
}

}  // namespace cpdd
