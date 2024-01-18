#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
struct ParallelKDtree<point>::simple_node {
  size_t size = 0;
  simple_node* left = nullptr;
  simple_node* right = nullptr;

  simple_node() {}
  simple_node( size_t sz ) : size( sz ), left( nullptr ), right( nullptr ) {}
  simple_node( simple_node* L, simple_node* R ) :
      size( L->size + R->size ), left( L ), right( R ) {}
};

template<typename point>
struct ParallelKDtree<point>::node {
  using coord = point::coord;
  using coords = point::coords;
  using points = parlay::sequence<point>;
  using slice = parlay::slice<point*, point*>;

  bool is_leaf;
  bool is_dummy;
  size_t size;
};

template<typename point>
struct ParallelKDtree<point>::leaf : node {
  points pts;
  leaf() : node{ true, false, static_cast<size_t>( 0 ) } {};
  leaf( slice In ) : node{ true, false, static_cast<size_t>( In.size() ) } {
    pts = points::uninitialized( LEAVE_WRAP );
    for ( int i = 0; i < In.size(); i++ ) {
      pts[i] = In[i];
    }
  }
  leaf( slice In, bool _is_dummy ) :
      node{ true, true, static_cast<size_t>( In.size() ) } {
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
      node{ false, false, _left->size + _right->size },
      left( _left ),
      right( _right ),
      split( _split ) {}
};

template<typename point>
typename ParallelKDtree<point>::leaf*
ParallelKDtree<point>::alloc_leaf_node( slice In ) {
  leaf* o = parlay::type_allocator<leaf>::alloc();
  new ( o ) leaf( In );
  assert( o->is_dummy == false );
  return o;
}

template<typename point>
typename ParallelKDtree<point>::leaf*
ParallelKDtree<point>::alloc_dummy_leaf( slice In ) {
  leaf* o = parlay::type_allocator<leaf>::alloc();
  new ( o ) leaf( In, true );
  assert( o->is_dummy == true );
  return o;
}

template<typename point>
typename ParallelKDtree<point>::leaf*
ParallelKDtree<point>::alloc_empty_leaf() {
  leaf* o = parlay::type_allocator<leaf>::alloc();
  new ( o ) leaf();
  assert( o->size == 0 && o->pts.size() == 0 );
  return o;
}

template<typename point>
typename ParallelKDtree<point>::interior*
ParallelKDtree<point>::alloc_interior_node( node* L, node* R, const splitter& split ) {
  interior* o = parlay::type_allocator<interior>::alloc();
  new ( o ) interior( L, R, split );
  return o;
}

template<typename point>
typename ParallelKDtree<point>::simple_node*
ParallelKDtree<point>::alloc_simple_node( simple_node* L, simple_node* R ) {
  simple_node* o = parlay::type_allocator<simple_node>::alloc();
  new ( o ) simple_node( L, R );
  return o;
}

template<typename point>
typename ParallelKDtree<point>::simple_node*
ParallelKDtree<point>::alloc_simple_node( size_t sz ) {
  simple_node* o = parlay::type_allocator<simple_node>::alloc();
  new ( o ) simple_node( sz );
  return o;
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
void
ParallelKDtree<point>::free_simple_node( simple_node* T ) {
  parlay::type_allocator<simple_node>::retire( T );
}

template<typename point>
inline size_t
ParallelKDtree<point>::get_imbalance_ratio() {
  if ( const auto env_p = std::getenv( "INBALANCE_RATIO" ) ) {
    return static_cast<size_t>( std::stoi( env_p ) );
  } else {
    return static_cast<size_t>( INBALANCE_RATIO );
  }
}

template<typename point>
inline bool
ParallelKDtree<point>::inbalance_node( const size_t l, const size_t n ) {
  if ( n == 0 ) return true;
  return Num::Gt( static_cast<size_t>( std::abs( 100.0 * l / n - 50.0 ) ),
                  get_imbalance_ratio() );
}

}  // namespace cpdd
