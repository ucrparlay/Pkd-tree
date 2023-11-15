#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

//* NN search
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::p2p_distance( const point& p, const point& q,
                                     const dim_type DIM ) {
    coord r = 0;
    for ( dim_type i = 0; i < DIM; i++ ) {
        r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
    }
    return std::move( r );
}

//* distance between a point and a box
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::p2b_distance( const point& p,
                                     const typename ParallelKDtree<point>::box& a,
                                     const dim_type DIM ) {
    coord r = 0;
    for ( dim_type i = 0; i < DIM; i++ ) {
        if ( Num::Lt( p.pnt[i], a.first.pnt[i] ) ) {
            r += ( a.first.pnt[i] - p.pnt[i] ) * ( a.first.pnt[i] - p.pnt[i] );
        } else if ( Num::Gt( p.pnt[i], a.second.pnt[i] ) ) {
            r += ( p.pnt[i] - a.second.pnt[i] ) * ( p.pnt[i] - a.second.pnt[i] );
        }
    }
    return r;
}

//* early return the partial distance between p and q if it is larger than r
//* else return the distance between p and q
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::interruptible_distance( const point& p, const point& q, coord up,
                                               dim_type DIM ) {
    coord r = 0;
    dim_type i = 0;
    if ( DIM >= 6 ) {
        while ( 1 ) {
            r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
            i++;
            r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
            i++;
            r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
            i++;
            r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
            i++;

            if ( Num::Gt( r, up ) ) {
                return r;
            }
            if ( i + 4 > DIM ) {
                break;
            }
        }
    }
    while ( i < DIM ) {
        r += ( p.pnt[i] - q.pnt[i] ) * ( p.pnt[i] - q.pnt[i] );
        i++;
    }
    return r;
}

template<typename point>
template<typename StoreType>
void
ParallelKDtree<point>::k_nearest( node* T, const point& q, const dim_type DIM,
                                  kBoundedQueue<point, StoreType>& bq,
                                  size_t& visNodeNum ) {
    visNodeNum++;

    if ( T->is_leaf ) {
        leaf* TL = static_cast<leaf*>( T );
        for ( int i = 0; i < TL->size; i++ ) {
            bq.insert(
                std::make_pair( std::ref( TL->pts[( !T->is_dummy ) * i] ),
                                p2p_distance( q, TL->pts[( !T->is_dummy ) * i], DIM ) ) );
        }
        return;
    }

    interior* TI = static_cast<interior*>( T );
    auto dx = [&]() -> coord { return TI->split.first - q.pnt[TI->split.second]; };

    k_nearest( Num::Gt( dx(), 0 ) ? TI->left : TI->right, q, DIM, bq, visNodeNum );
    // TODO change to compare arbirtrary box
    if ( Num::Gt( dx() * dx(), bq.top().second ) && bq.full() ) {
        return;
    }
    k_nearest( Num::Gt( dx(), 0 ) ? TI->right : TI->left, q, DIM, bq, visNodeNum );
}

template<typename point>
template<typename StoreType>
void
ParallelKDtree<point>::k_nearest( node* T, const point& q, const dim_type DIM,
                                  kBoundedQueue<point, StoreType>& bq, const box& nodeBox,
                                  size_t& visNodeNum ) {
    visNodeNum++;

    if ( T->is_leaf ) {
        leaf* TL = static_cast<leaf*>( T );
        int i = 0;
        while ( !bq.full() && i < TL->size ) {
            bq.insert(
                std::make_pair( std::ref( TL->pts[( !T->is_dummy ) * i] ),
                                p2p_distance( q, TL->pts[( !T->is_dummy ) * i], DIM ) ) );
            i++;
        }
        while ( i < TL->size ) {
            coord r = interruptible_distance( q, TL->pts[( !T->is_dummy ) * i],
                                              bq.top_value(), DIM );
            if ( Num::Leq( r, bq.top_value() ) ) {
                bq.insert(
                    std::make_pair( std::ref( TL->pts[( !T->is_dummy ) * i] ), r ) );
            }
            i++;
        }
        return;
    }

    interior* TI = static_cast<interior*>( T );
    auto go_left = [&]() -> bool {
        return Num::Gt( TI->split.first - q.pnt[TI->split.second], 0 );
    };

    box firstBox( nodeBox ), secondBox( nodeBox );

    if ( go_left() ) {  //* go left child
        firstBox.second.pnt[TI->split.second] = TI->split.first;
        secondBox.first.pnt[TI->split.second] = TI->split.first;
    } else {  //* go right child
        firstBox.first.pnt[TI->split.second] = TI->split.first;
        secondBox.second.pnt[TI->split.second] = TI->split.first;
    }

    k_nearest( go_left() ? TI->left : TI->right, q, DIM, bq, firstBox, visNodeNum );
    if ( Num::Gt( p2b_distance( q, secondBox, DIM ), bq.top_value() ) && bq.full() ) {
        return;
    }
    k_nearest( go_left() ? TI->right : TI->left, q, DIM, bq, secondBox, visNodeNum );
    return;
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
        TI->size >= SERIAL_BUILD_CUTOFF,
        [&] { l = range_count_value( TI->left, queryBox, lbox ); },
        [&] { r = range_count_value( TI->right, queryBox, rbox ); } );

    return std::move( l + r );
}

template<typename point>
ParallelKDtree<point>::simple_node*
ParallelKDtree<point>::range_count_save_path( node* T, const box& queryBox,
                                              const box& nodeBox ) {
    if ( !intersect_box( nodeBox, queryBox ) ) return alloc_simple_node( 0 );
    if ( within_box( nodeBox, queryBox ) ) {
        return alloc_simple_node( T->size );
    }

    if ( T->is_leaf ) {
        size_t cnt = 0;
        leaf* TL = static_cast<leaf*>( T );
        for ( int i = 0; i < TL->size; i++ ) {
            if ( within_box( TL->pts[( !T->is_dummy ) * i], queryBox ) ) {
                cnt++;
            }
        }
        return alloc_simple_node( cnt );
    }

    interior* TI = static_cast<interior*>( T );
    box lbox( nodeBox ), rbox( nodeBox );
    lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
    rbox.first.pnt[TI->split.second] = TI->split.first;

    simple_node *L, *R;
    parlay::par_do_if(
        TI->size >= SERIAL_BUILD_CUTOFF,
        [&] { L = range_count_save_path( TI->left, queryBox, lbox ); },
        [&] { R = range_count_save_path( TI->right, queryBox, rbox ); } );

    return alloc_simple_node( L, R );
}

template<typename point>
template<typename Slice>
size_t
ParallelKDtree<point>::range_query( const typename ParallelKDtree<point>::box& queryBox,
                                    Slice Out ) {
    auto tree = range_count_save_path( this->root, queryBox, this->bbox );

    return tree->size;
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
