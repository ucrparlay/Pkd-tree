#pragma once

#include "../kdTreeParallel.h"
#include "inner_tree.hpp"
#include "parlay/slice.h"

namespace cpdd {
template<typename point>
void ParallelKDtree<point>::pointInsert(const point p, const dim_type DIM) {
    if (this->root == nullptr) {
        parlay::sequence<point> pts(1);
        pts[0] = p;
        return build(parlay::make_slice(pts), DIM);
    }
    node* T = this->root;
    this->bbox = get_box(this->bbox, box(p, p));
    dim_type d = T->is_leaf ? 0 : static_cast<interior*>(T)->split.second;
    this->root = pointInsert_recursive(T, p, d, DIM);
    return;
}

template<typename point>
typename ParallelKDtree<point>::node* ParallelKDtree<point>::pointInsert_recursive(node* T, const point& p, dim_type d,
                                                                                   const dim_type DIM) {
    if (T->is_leaf) {
        leaf* TL = static_cast<leaf*>(T);
        if (TL->is_dummy && TL->pts[0] == p) {
            T->size++;
            return T;
        } else if (!TL->is_dummy && TL->size + 1 <= this->LEAVE_WRAP) {
            TL->pts[TL->size] = p;
            TL->size++;
            return T;
        } else {
            parlay::sequence<point> pts{p};
            return rebuild_with_insert(T, parlay::make_slice(pts), d, DIM);
        }
    }

    interior* TI = static_cast<interior*>(T);

    bool go_left = Num::Lt(p.pnt[TI->split.second], TI->split.first) ? true : false;
    node* nxt_node = go_left ? TI->left : TI->right;
    if (!inbalance_node(nxt_node->size + 1, TI->size + 1)) {
        d = (d + 1) % DIM;
        auto return_node = pointInsert_recursive(nxt_node, p, d, DIM);
        if (go_left) {
            update_interior(T, return_node, TI->right);
        } else {
            update_interior(T, TI->left, return_node);
        }
        return TI;
    } else {
        parlay::sequence<point> pts{p};
        return rebuild_with_insert(T, parlay::make_slice(pts), d, DIM);  // NOTE: the rebuild is in parallel
    }
}

template<typename point>
void ParallelKDtree<point>::batchInsert(slice A, const dim_type DIM) {
    if (this->root == nullptr) {
        return build(A, DIM);
    }

    points B = points::uninitialized(A.size());
    node* T = this->root;
    /*box b = get_box(A);*/
    this->bbox = get_box(this->bbox, get_box(A));
    dim_type d = T->is_leaf ? 0 : static_cast<interior*>(T)->split.second;
    this->root = batchInsert_recusive(T, A, B.cut(0, A.size()), d, DIM);
    assert(this->root != NULL);
    return;
}

template<typename point>
typename ParallelKDtree<point>::node* ParallelKDtree<point>::update_inner_tree_by_tag(
    bucket_type idx, const node_tags& tags, parlay::sequence<node*>& treeNodes, bucket_type& p,
    const tag_nodes& rev_tag) {

    if (tags[idx].second == BUCKET_NUM + 1 || tags[idx].second == BUCKET_NUM + 2) {
        assert(rev_tag[p] == idx);
        return treeNodes[p++];
    }

    assert(tags[idx].second == BUCKET_NUM);
    assert(tags[idx].first != NULL);
    node *L, *R;
    L = update_inner_tree_by_tag(idx << 1, tags, treeNodes, p, rev_tag);
    R = update_inner_tree_by_tag(idx << 1 | 1, tags, treeNodes, p, rev_tag);
    update_interior(tags[idx].first, L, R);
    return tags[idx].first;
}

template<typename point>
typename ParallelKDtree<point>::node* ParallelKDtree<point>::rebuild_with_insert(node* T, slice In, const dim_type d,
                                                                                 const dim_type DIM) {
    uint_fast8_t curDim = pick_rebuild_dim(T, d, DIM);
    points wo = points::uninitialized(T->size + In.size());
    points wx = points::uninitialized(T->size + In.size());
    parlay::parallel_for(0, In.size(), [&](size_t j) { wx[j] = In[j]; });
    flatten(T, wx.cut(In.size(), wx.size()));
    delete_tree_recursive(T);
    return build_recursive(parlay::make_slice(wx), parlay::make_slice(wo), curDim, DIM,
                           get_box(parlay::make_slice(wx)));
}

//* return the updated node
template<typename point>
typename ParallelKDtree<point>::node* ParallelKDtree<point>::batchInsert_recusive(node* T, slice In, slice Out,
                                                                                  dim_type d, const dim_type DIM) {
    size_t n = In.size();

    if (n == 0) return T;

    if (T->is_leaf) {
        leaf* TL = static_cast<leaf*>(T);
        if (!TL->is_dummy && n + TL->size <= LEAVE_WRAP) {
            assert(T->size == TL->size);
            if (TL->pts.size() == 0) {
                TL->pts = points::uninitialized(LEAVE_WRAP);
            }
            for (int i = 0; i < n; i++) {
                TL->pts[TL->size + i] = In[i];
            }
            TL->size += n;
            return T;
        } else {
            return rebuild_with_insert(T, In, d, DIM);
        }
    }

    if (n <= SERIAL_BUILD_CUTOFF) {
        interior* TI = static_cast<interior*>(T);
        auto _2ndGroup = std::ranges::partition(
            In, [&](const point& p) { return Num::Lt(p.pnt[TI->split.second], TI->split.first); });

        //* rebuild
        if (inbalance_node(TI->left->size + _2ndGroup.begin() - In.begin(), TI->size + n)) {
            return rebuild_with_insert(T, In, d, DIM);
        }
        //* continue
        node *L, *R;
        d = (d + 1) % DIM;
        L = batchInsert_recusive(TI->left, In.cut(0, _2ndGroup.begin() - In.begin()),
                                 Out.cut(0, _2ndGroup.begin() - In.begin()), d, DIM);
        R = batchInsert_recusive(TI->right, In.cut(_2ndGroup.begin() - In.begin(), n),
                                 Out.cut(_2ndGroup.begin() - In.begin(), n), d, DIM);
        update_interior(T, L, R);
        assert(T->size == L->size + R->size && TI->split.second >= 0 && TI->is_leaf == false);
        return T;
    }

    //@ assign each node a tag
    InnerTree IT;
    IT.init();
    assert(IT.rev_tag.size() == BUCKET_NUM);
    IT.assign_node_tag(T, 1);
    assert(IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM);

    seieve_points(In, Out, n, IT.tags, IT.sums, IT.tagsNum);

    IT.tag_inbalance_node();
    assert(IT.tagsNum > 0 && IT.tagsNum <= BUCKET_NUM);
    auto treeNodes = parlay::sequence<node*>::uninitialized(IT.tagsNum);

    parlay::parallel_for(
        0, IT.tagsNum,
        [&](size_t i) {
            size_t s = 0;
            for (int j = 0; j < i; j++) {
                s += IT.sums_tree[IT.rev_tag[j]];
            }

            dim_type nextDim = (d + IT.get_depth_by_index(IT.rev_tag[i])) % DIM;
            if (IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 1) {  // NOTE: continue sieve
                treeNodes[i] =
                    batchInsert_recusive(IT.tags[IT.rev_tag[i]].first, Out.cut(s, s + IT.sums_tree[IT.rev_tag[i]]),
                                         In.cut(s, s + IT.sums_tree[IT.rev_tag[i]]), nextDim, DIM);
            } else {  // NOTE: launch rebuild subtree
                assert(IT.tags[IT.rev_tag[i]].second == BUCKET_NUM + 2);
                assert(IT.tags[IT.rev_tag[i]].first->size + IT.sums_tree[IT.rev_tag[i]] >= 0);

                treeNodes[i] = rebuild_with_insert(IT.tags[IT.rev_tag[i]].first,
                                                   Out.cut(s, s + IT.sums_tree[IT.rev_tag[i]]), nextDim, DIM);
            }
        },
        1);

    bucket_type beatles = 0;
    return update_inner_tree_by_tag(1, IT.tags, treeNodes, beatles, IT.rev_tag);
}

}  // namespace cpdd
