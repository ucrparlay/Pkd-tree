#pragma once

#include "../kdTreeParallel.h"
#include "inner_tree.hpp"
#include <ranges>
#include <utility>

namespace cpdd {
template<typename point>
// NOTE: delete one point from the tree
void ParallelKDtree<point>::pointDelete(const point p, const dim_type DIM) {
    node* T = this->root;
    box bx = this->bbox;
    dim_type d = T->is_leaf ? 0 : static_cast<interior*>(T)->split.second;
    std::tie(this->root, this->bbox) = pointDelete_recursive(T, bx, p, d, DIM, 1);
}

// NOTE: implementaion of pointDelete
template<typename point>
typename ParallelKDtree<point>::node_box ParallelKDtree<point>::pointDelete_recursive(node* T, const box& bx,
                                                                                      const point& p, dim_type d,
                                                                                      const dim_type DIM,
                                                                                      bool hasTomb) {
    if (T->is_leaf) {
        leaf* TL = static_cast<leaf*>(T);

        if (TL->is_dummy) {
            assert(static_cast<leaf*>(T)->pts[0] == p);
            T->size--;
            assert(get_box(T) == box(p, p));
            return node_box(T, box(p, p));
        }

        auto it = std::ranges::find(TL->pts.begin(), TL->pts.begin() + TL->size, p);
        assert(it != TL->pts.begin() + TL->size);
        std::ranges::iter_swap(it, TL->pts.begin() + TL->size - 1);
        TL->size--;
        assert(TL->size >= 0);
        return node_box(T, get_box(TL->pts.cut(0, TL->size)));
    }

    interior* TI = static_cast<interior*>(T);

    // NOTE: seieve the point
    dim_type next_dim = (d + 1) % DIM;
    box lbox(bx), rbox(bx), new_box;
    lbox.second.pnt[TI->split.second] = TI->split.first;  // PERF: loose
    rbox.first.pnt[TI->split.second] = TI->split.first;
    bool putTomb;

    if (Num::Lt(p.pnt[TI->split.second], TI->split.first)) {  // NOTE: go left
        putTomb = hasTomb && inbalance_node(TI->left->size - 1, TI->size - 1);
        hasTomb = putTomb ? false : hasTomb;
        auto [L, Lbox] = pointDelete_recursive(TI->left, lbox, p, next_dim, DIM, hasTomb);
        update_interior(T, L, TI->right);
        new_box = get_box(Lbox, rbox);
    } else {  // NOTE: go right
        putTomb = hasTomb && inbalance_node(TI->right->size - 1, TI->size - 1);
        hasTomb = putTomb ? false : hasTomb;
        auto [R, Rbox] = pointDelete_recursive(TI->right, rbox, p, next_dim, DIM, hasTomb);
        update_interior(T, TI->left, R);
        new_box = get_box(lbox, Rbox);
    }

    // NOTE: rebuild if necessary
    if (putTomb) {
        return rebuild_single_tree(T, d, DIM, true);
    }

    return node_box(T, new_box);
}

// NOTE: default batch delete
template<typename point>
void ParallelKDtree<point>::batchDelete(slice A, const dim_type DIM) {
    batchDelete(A, DIM, FullCoveredTag());
    // batchDelete(A, DIM, PartialCoverTag());
    return;
}

// NOTE: assume all points are fully covered in the tree
template<typename point>
void ParallelKDtree<point>::batchDelete(slice A, const dim_type DIM, FullCoveredTag) {
    points B = points::uninitialized(A.size());
    node* T = this->root;
    box bx = this->bbox;
    dim_type d = T->is_leaf ? 0 : static_cast<interior*>(T)->split.second;
    std::tie(this->root, this->bbox) =
        batchDelete_recursive(T, bx, A, parlay::make_slice(B), d, DIM, 1, FullCoveredTag());
    return;
}

// NOTE: batch delete suitable for points that are pratially covered in the tree
template<typename point>
void ParallelKDtree<point>::batchDelete(slice A, const dim_type DIM, PartialCoverTag) {
    points B = points::uninitialized(A.size());
    node* T = this->root;
    box bx = this->bbox;
    dim_type d = T->is_leaf ? 0 : static_cast<interior*>(T)->split.second;
    // NOTE: first sieve the points
    std::tie(T, this->bbox) = batchDelete_recursive(T, bx, A, parlay::make_slice(B), d, DIM, PartialCoverTag());
    // NOTE: then rebuild the tree with full parallelsim
    std::tie(this->root, bx) = rebuild_tree_recursive(T, d, DIM, false);
    assert(bx == this->bbox);

    return;
}

// NOTE: the node which needs to be rebuilt has tag BUCKET_NUM+3
// NOTE: the bucket node whose ancestor has been rebuilt has tag BUCKET_NUM+2
// NOTE: the bucket node whose ancestor has not been ... has BUCKET_NUM+1
// NOTE: otherwise, it's BUCKET_NUM
template<typename point>
typename ParallelKDtree<point>::node_box ParallelKDtree<point>::delete_inner_tree(
    bucket_type idx, const node_tags& tags, parlay::sequence<node_box>& treeNodes, bucket_type& p,
    const tag_nodes& rev_tag, dim_type d, const dim_type DIM) {
    if (tags[idx].second == BUCKET_NUM + 1 || tags[idx].second == BUCKET_NUM + 2) {
        assert(rev_tag[p] == idx);
        assert(tags[idx].second == BUCKET_NUM + 1 ||
               tags[idx].first->size > SERIAL_BUILD_CUTOFF == static_cast<interior*>(tags[idx].first)->aug_flag);
        return treeNodes[p++];  // WARN: this blocks the parallelsim
    }

    auto [L, Lbox] = delete_inner_tree(idx << 1, tags, treeNodes, p, rev_tag, (d + 1) % DIM, DIM);
    auto [R, Rbox] = delete_inner_tree(idx << 1 | 1, tags, treeNodes, p, rev_tag, (d + 1) % DIM, DIM);

    assert(tags[idx].first->size > SERIAL_BUILD_CUTOFF == static_cast<interior*>(tags[idx].first)->aug_flag);
    update_interior(tags[idx].first, L, R);

    if (tags[idx].second == BUCKET_NUM + 3) {  // NOTE: launch rebuild
        interior const* TI = static_cast<interior*>(tags[idx].first);
        assert(inbalance_node(TI->left->size, TI->size) || TI->size < THIN_LEAVE_WRAP);

        if (tags[idx].first->size == 0) {  // NOTE: special judge for empty tree
            delete_tree_recursive(tags[idx].first, false);
            return node_box(alloc_empty_leaf(), get_empty_box());
        }

        return rebuild_single_tree(tags[idx].first, d, DIM, false);
    }

    return node_box(tags[idx].first, get_box(Lbox, Rbox));
}

// NOTE: delete with rebuild, with the assumption that all points are in the tree
// WARN: the param d can be only used when rotate cutting is applied
template<typename point>
typename ParallelKDtree<point>::node_box ParallelKDtree<point>::batchDelete_recursive(
    node* T, const typename ParallelKDtree<point>::box& bx, slice In, slice Out, dim_type d, const dim_type DIM,
    bool hasTomb, FullCoveredTag) {
    size_t n = In.size();

    if (n == 0) {
        assert(within_box(get_box(T), bx));
        return node_box(T, bx);
    }

    // INFO: may can be used to accelerate the whole deletion process
    // if ( n == T->size ) {
    //     if ( hasTomb ) {
    //         delete_tree_recursive( T );
    //         return node_box( alloc_empty_leaf(), get_empty_box() );
    //     }
    //     T->size = 0;  //* lazy mark
    //     return node_box( T, get_empty_box() );
    // }

    if (T->is_leaf) {
        assert(T->size >= In.size());
        leaf* TL = static_cast<leaf*>(T);

        if (TL->is_dummy) {
            assert(T->is_leaf);
            assert(In.size() <= T->size);  // WARN: cannot delete more points then there are
            T->size -= In.size();          // WARN: this assumes that In\in T
            return node_box(T, box(TL->pts[0], TL->pts[0]));
        }

        auto it = TL->pts.begin(), end = TL->pts.begin() + TL->size;
        for (int i = 0; i < In.size(); i++) {
            it = std::ranges::find(TL->pts.begin(), end, In[i]);
            assert(it != end);
            std::ranges::iter_swap(it, --end);
        }

        assert(std::distance(TL->pts.begin(), end) == TL->size - In.size());
        TL->size -= In.size();
        assert(TL->size >= 0);
        return node_box(T, get_box(TL->pts.cut(0, TL->size)));
    }

    if (In.size() <= SERIAL_BUILD_CUTOFF) {
        // if (In.size() <= 32) {
        // if (0) {
        interior* TI = static_cast<interior*>(T);
        auto _2ndGroup = std::ranges::partition(
            In, [&](const point& p) { return Num::Lt(p.pnt[TI->split.second], TI->split.first); });

        bool putTomb =
            hasTomb && (inbalance_node(TI->left->size - (_2ndGroup.begin() - In.begin()), TI->size - In.size()) ||
                        TI->size - In.size() < THIN_LEAVE_WRAP);
        hasTomb = putTomb ? false : hasTomb;
        assert(putTomb ? (!hasTomb) : true);

        assert(this->_split_rule == MAX_STRETCH_DIM || (this->_split_rule == ROTATE_DIM && d == TI->split.second));
        dim_type nextDim = (d + 1) % DIM;

        box lbox(bx), rbox(bx);
        lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
        rbox.first.pnt[TI->split.second] = TI->split.first;

        auto [L, Lbox] =
            batchDelete_recursive(TI->left, lbox, In.cut(0, _2ndGroup.begin() - In.begin()),
                                  Out.cut(0, _2ndGroup.begin() - In.begin()), nextDim, DIM, hasTomb, FullCoveredTag());
        auto [R, Rbox] =
            batchDelete_recursive(TI->right, rbox, In.cut(_2ndGroup.begin() - In.begin(), n),
                                  Out.cut(_2ndGroup.begin() - In.begin(), n), nextDim, DIM, hasTomb, FullCoveredTag());

        TI->aug_flag = hasTomb ? false : TI->size > this->SERIAL_BUILD_CUTOFF;
        update_interior(T, L, R);
        assert(T->size == L->size + R->size && TI->split.second >= 0 && TI->is_leaf == false);

        // NOTE: rebuild
        if (putTomb) {
            assert(TI->size == T->size);
            assert(inbalance_node(TI->left->size, TI->size) || TI->size < THIN_LEAVE_WRAP);
            return rebuild_single_tree(T, d, DIM, false);
        }

        return node_box(T, get_box(Lbox, Rbox));
    }

    InnerTree IT;
    IT.init();
    IT.assign_node_tag(T, 1);
    assert(IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM);
    seieve_points(In, Out, n, IT.tags, IT.sums, IT.tagsNum);

    auto treeNodes = parlay::sequence<node_box>::uninitialized(IT.tagsNum);
    auto boxs = parlay::sequence<box>::uninitialized(IT.tagsNum);

    IT.tag_inbalance_node_deletion(boxs, bx, hasTomb);

    parlay::parallel_for(
        0, IT.tagsNum,
        [&](size_t i) {
            size_t start = 0;
            for (int j = 0; j < i; j++) {
                start += IT.sums[j];
            }

            assert(IT.sums_tree[IT.rev_tag[i]] == IT.sums[i]);
            assert(IT.tags[IT.rev_tag[i]].first->size >= IT.sums[i]);
            assert(within_box(get_box(Out.cut(start, start + IT.sums[i])), get_box(IT.tags[IT.rev_tag[i]].first)));

            dim_type nextDim = (d + IT.get_depth_by_index(IT.rev_tag[i])) % DIM;

            treeNodes[i] =
                batchDelete_recursive(IT.tags[IT.rev_tag[i]].first, boxs[i], Out.cut(start, start + IT.sums[i]),
                                      In.cut(start, start + IT.sums[i]), nextDim, DIM,
                                      IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 1, FullCoveredTag());
        },
        1);

    bucket_type beatles = 0;
    return delete_inner_tree(1, IT.tags, treeNodes, beatles, IT.rev_tag, d, DIM);
}

// NOTE: only sieve the points, without rebuilding the tree
template<typename point>
typename ParallelKDtree<point>::node_box ParallelKDtree<point>::batchDelete_recursive(
    node* T, const typename ParallelKDtree<point>::box& bx, slice In, slice Out, dim_type d, const dim_type DIM,
    PartialCoverTag) {
    size_t n = In.size();

    if (n == 0) return node_box(T, bx);

    if (T->is_leaf) {
        leaf* TL = static_cast<leaf*>(T);

        if (TL->is_dummy) {  // NOTE: need to check whether all points are in the leaf
            assert(T->is_leaf);

            // PERF: slow when In.size() is large
            for (size_t i = 0; TL->size && i < In.size(); i++) {
                if (TL->pts[0] == In[i]) {
                    TL->size -= 1;
                }
            }
            assert(TL->size >= 0);
            return node_box(T, box(TL->pts[0], TL->pts[0]));
        }

        auto it = TL->pts.begin(), end = TL->pts.begin() + TL->size;
        for (int i = 0; TL->size && i < In.size(); i++) {
            it = std::ranges::find(TL->pts.begin(), end, In[i]);
            if (it != end) {  // NOTE: find a point
                std::ranges::iter_swap(it, --end);
                TL->size -= 1;
            }
        }
        return node_box(T, get_box(TL->pts.cut(0, TL->size)));
    }

    if (In.size() <= SERIAL_BUILD_CUTOFF) {
        // if (In.size()) {
        interior* TI = static_cast<interior*>(T);
        auto _2ndGroup = std::ranges::partition(
            In, [&](const point& p) { return Num::Lt(p.pnt[TI->split.second], TI->split.first); });

        dim_type nextDim = (d + 1) % DIM;

        box lbox(bx), rbox(bx);
        lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
        rbox.first.pnt[TI->split.second] = TI->split.first;

        auto [L, Lbox] =
            batchDelete_recursive(TI->left, lbox, In.cut(0, _2ndGroup.begin() - In.begin()),
                                  Out.cut(0, _2ndGroup.begin() - In.begin()), nextDim, DIM, PartialCoverTag());
        auto [R, Rbox] =
            batchDelete_recursive(TI->right, rbox, In.cut(_2ndGroup.begin() - In.begin(), n),
                                  Out.cut(_2ndGroup.begin() - In.begin(), n), nextDim, DIM, PartialCoverTag());

        update_interior(T, L, R);
        assert(T->size == L->size + R->size && TI->split.second >= 0 && TI->is_leaf == false);

        return node_box(T, get_box(Lbox, Rbox));
    }

    InnerTree IT;
    IT.init();
    IT.assign_node_tag(T, 1);
    assert(IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM);
    seieve_points(In, Out, n, IT.tags, IT.sums, IT.tagsNum);

    auto treeNodes = parlay::sequence<node_box>::uninitialized(IT.tagsNum);
    auto boxs = parlay::sequence<box>::uninitialized(IT.tagsNum);

    // NOTE: never set tomb, this equivalent to only calcualte the bounding box,
    IT.tag_inbalance_node_deletion(boxs, bx, false);

    parlay::parallel_for(
        0, IT.tagsNum,
        // NOTE: i is the index of the tags
        [&](size_t i) {
            // assert( IT.sums_tree[IT.rev_tag[i]] == IT.sums[i] );
            size_t start = 0;
            for (int j = 0; j < i; j++) {
                start += IT.sums[j];
            }

            dim_type nextDim = (d + IT.get_depth_by_index(IT.rev_tag[i])) % DIM;
            treeNodes[i] =
                batchDelete_recursive(IT.tags[IT.rev_tag[i]].first, boxs[i], Out.cut(start, start + IT.sums[i]),
                                      In.cut(start, start + IT.sums[i]), nextDim, DIM, PartialCoverTag());
        },
        1);

    bucket_type beatles = 0;
    return update_inner_tree(1, IT.tags, treeNodes, beatles, IT.rev_tag);
}

}  // namespace cpdd
