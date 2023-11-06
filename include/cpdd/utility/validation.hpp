#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
bool
ParallelKDtree<point>::checkBox( node* T, const box& bx ) {
  assert( T != nullptr );
  assert( legal_box( bx ) );
  points wx = points::uninitialized( T->size );
  flatten( T, parlay::make_slice( wx ) );
  auto b = get_box( parlay::make_slice( wx ) );
  // LOG << b.first << b.second << ENDL;
  return within_box( b, bx );
}

template<typename point>
size_t
ParallelKDtree<point>::checkSize( node* T ) {
  if ( T->is_leaf ) {
    return T->size;
  }
  interior* TI = static_cast<interior*>( T );
  size_t l = checkSize( TI->left );
  size_t r = checkSize( TI->right );
  assert( l + r == T->size );
  return T->size;
}

template<typename point>
void
ParallelKDtree<point>::checkTreeSameSequential( node* T, int dim, const int& DIM ) {
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

template<typename point>
void
ParallelKDtree<point>::validate( const dim_type DIM ) {
  if ( checkBox( this->root, this->bbox ) && legal_box( this->bbox ) ) {
    std::cout << "Correct bounding box" << std::endl << std::flush;
  } else {
    std::cout << "wrong bounding box" << std::endl << std::flush;
    abort();
  }

  // if ( this->_split_rule == ROTATE_DIM ) {
  //   checkTreeSameSequential( this->root, 0, DIM );
  //   std::cout << "Correct rotate dimension" << std::endl << std::flush;
  // }

  if ( checkSize( this->root ) == this->root->size ) {
    std::cout << "Correct size" << std::endl << std::flush;
  } else {
    std::cout << "wrong tree size" << std::endl << std::flush;
    abort();
  }
  return;
}

template<typename point>
int
ParallelKDtree<point>::getTreeHeight( node* T, int deep ) {
  if ( T->is_leaf ) {
    return deep;
  }
  interior* TI = static_cast<interior*>( T );
  int l = getTreeHeight( TI->left, deep + 1 );
  int r = getTreeHeight( TI->right, deep + 1 );
  return std::max( l, r );
}

template<typename point>
void
ParallelKDtree<point>::countTreeHeights( node* T, int deep, int& idx,
                                         parlay::sequence<int>& heights ) {
  if ( T->is_leaf ) {
    heights[idx++] = deep;
    return;
  }
  interior* TI = static_cast<interior*>( T );
  countTreeHeights( TI->left, deep + 1, idx, heights );
  countTreeHeights( TI->right, deep + 1, idx, heights );
  return;
}

}  // namespace cpdd