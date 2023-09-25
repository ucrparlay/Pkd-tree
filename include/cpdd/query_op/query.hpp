#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

//* NN search
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::ppDistanceSquared( const point& p, const point& q,
                                          const dim_type DIM ) {
  coord r = 0;
  for ( int i = 0; i < DIM; i++ ) {
    r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
  }
  return std::move( r );
}

//? parallel query
template<typename point>
void
ParallelKDtree<point>::k_nearest( node* T, const point& q, const dim_type DIM,
                                  kBoundedQueue<point>& bq, size_t& visNodeNum ) {
  visNodeNum++;

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      bq.insert(
          std::make_pair( &( TL->pts[( !T->is_dummy ) * i] ),
                          ppDistanceSquared( q, TL->pts[( !T->is_dummy ) * i], DIM ) ) );
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  auto dx = [&]() { return TI->split.first - q.pnt[TI->split.second]; };

  k_nearest( Num::Gt( dx(), 0 ) ? TI->left : TI->right, q, DIM, bq, visNodeNum );
  if ( Num::Gt( dx() * dx(), bq.top().second ) && bq.full() ) {
    return;
  }
  k_nearest( Num::Gt( dx(), 0 ) ? TI->right : TI->left, q, DIM, bq, visNodeNum );
}

//* range count
template<typename point>
size_t
ParallelKDtree<point>::range_count( const typename ParallelKDtree<point>::box& bx ) {
  return range_count_value( this->root, bx, this->bbox );
}

template<typename point>
size_t
ParallelKDtree<point>::range_count_value( node* T, const box& queryBox,
                                          const box& nodeBox ) {
  if ( !intersect_box( nodeBox, queryBox ) ) return 0;
  if ( within_box( nodeBox, queryBox ) ) return T->size;

  if ( T->is_leaf ) {
    size_t cnt = 0;
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        cnt++;
      }
    }
    return std::move( cnt );
  }

  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  size_t l, r;
  parlay::par_do_if(
      // TI->size >= SERIAL_BUILD_CUTOFF,
      0, [&] { l = range_count_value( TI->left, queryBox, lbox ); },
      [&] { r = range_count_value( TI->right, queryBox, rbox ); } );

  return std::move( l + r );
}

//* range count
// TODO change return type to pointers
template<typename point>
template<typename Slice>
size_t
ParallelKDtree<point>::range_query( const typename ParallelKDtree<point>::box& bx,
                                    Slice Out ) {
  size_t s = 0;
  range_query_recursive( this->root, Out, s, bx, this->bbox );
  return s;
}

template<typename point>
template<typename Slice>
void
ParallelKDtree<point>::range_query_recursive( node* T, Slice Out, size_t& s,
                                              const box& queryBox, const box& nodeBox ) {
  if ( !intersect_box( nodeBox, queryBox ) ) {
    return;
  }

  if ( within_box( nodeBox, queryBox ) ) {
    flatten( T, Out.cut( s, s + T->size ) );
    s += T->size;
    return;
  }

  if ( T->is_leaf ) {
    leaf* TL = static_cast<leaf*>( T );
    for ( int i = 0; i < TL->size; i++ ) {
      if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
        Out[s++] = TL->pts[( !T->is_dummy ) * i];
      }
    }
    return;
  }

  interior* TI = static_cast<interior*>( T );
  box lbox( nodeBox ), rbox( nodeBox );
  lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
  rbox.first.pnt[TI->split.second] = TI->split.first;

  range_query_recursive( TI->left, Out, s, queryBox, lbox );
  range_query_recursive( TI->right, Out, s, queryBox, rbox );

  return;
}

}  // namespace cpdd
