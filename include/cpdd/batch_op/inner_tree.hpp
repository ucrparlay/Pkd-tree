#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
struct ParallelKDtree<point>::InnerTree {
    //@ helpers
    bool assert_size(node* T) const {
        if (T->is_leaf) {
            leaf* TI = static_cast<leaf*>(T);
            assert(T->size <= TI->pts.size() && T->size <= LEAVE_WRAP);
            return true;
        }
        interior* TI = static_cast<interior*>(T);
        assert(TI->size == TI->left->size + TI->right->size);
        return true;
    }

    void assert_size_by_idx(bucket_type idx) const {
        if (idx > PIVOT_NUM || tags[idx].first->is_leaf) return;
        interior* TI = static_cast<interior*>(tags[idx].first);
        assert(TI->size == TI->left->size + TI->right->size);
        return;
    }

    inline bucket_type get_node_idx(bucket_type idx, node* T) {
        if (tags[idx].first == T) return idx;
        if (idx > PIVOT_NUM || tags[idx].first->is_leaf) return -1;
        auto pos = get_node_idx(idx << 1, T);
        if (pos != -1) return pos;
        return get_node_idx(idx << 1 | 1, T);
    }

    //@ cores
    inline void reset_tags_num() { tagsNum = 0; }

    // NOTE: Each node in the skeleton receives a tag
    // NOTE: A leaf node receives the tag < BUCKETNUM
    // NOTE: All internal node has tag == BUCKETNUM
    void assign_node_tag(node* T, bucket_type idx) {
        if (T->is_leaf || idx > PIVOT_NUM) {
            assert(tagsNum < BUCKET_NUM);
            tags[idx] = node_tag(T, tagsNum);
            rev_tag[tagsNum++] = idx;
            return;
        }
        // INFO: BUCKET ID in [0, BUCKET_NUM)
        tags[idx] = node_tag(T, BUCKET_NUM);
        interior* TI = static_cast<interior*>(T);
        assign_node_tag(TI->left, idx << 1);
        assign_node_tag(TI->right, idx << 1 | 1);
        return;
    }

    void reduce_sums(bucket_type idx) const {
        if (idx > PIVOT_NUM || tags[idx].first->is_leaf) {
            assert(tags[idx].second < BUCKET_NUM);
            sums_tree[idx] = sums[tags[idx].second];
            return;
        }
        reduce_sums(idx << 1);
        reduce_sums(idx << 1 | 1);
        sums_tree[idx] = sums_tree[idx << 1] + sums_tree[idx << 1 | 1];
        return;
    }

    dim_type get_depth_by_index(bucket_type tag) {
        dim_type h = 0;
        while (tag > 1) {
            tag >>= 1;
            ++h;
        }
        return h;
    }

    void pick_tag(bucket_type idx) {
        if (idx > PIVOT_NUM || tags[idx].first->is_leaf) {
            tags[idx].second = BUCKET_NUM + 1;
            rev_tag[tagsNum++] = idx;
            return;
        }
        assert(tags[idx].second == BUCKET_NUM && (!tags[idx].first->is_leaf));
        interior* TI = static_cast<interior*>(tags[idx].first);

        if (inbalance_node(TI->left->size + sums_tree[idx << 1],
                           TI->size + sums_tree[idx])) {
            tags[idx].second = BUCKET_NUM + 2;
            rev_tag[tagsNum++] = idx;
            return;
        }
        pick_tag(idx << 1);
        pick_tag(idx << 1 | 1);
        return;
    }

    void tag_inbalance_node() {
        reduce_sums(1);
        reset_tags_num();
        pick_tag(1);
        assert(assert_size(tags[1].first));
        return;
    }

    // NOTE: the node which needs to be rebuilt has tag BUCKET_NUM+3
    // the bucket node whose ancestor has been rebuilt has tag BUCKET_NUM+2
    // the bucket node whose ancestor has not been ... has BUCKET_NUM+1
    // otherwise, it's BUCKET_NUM
    void mark_tomb(bucket_type idx, box_s& boxs, box bx, bool hasTomb) {
        if (idx > PIVOT_NUM || tags[idx].first->is_leaf) {
            assert(tags[idx].second >= 0 && tags[idx].second < BUCKET_NUM);
            // tags[idx].second = (!hasTomb) ? BUCKET_NUM + 2 : BUCKET_NUM + 1;
            if (!hasTomb) {  // NOTE: this subtree needs to be rebuilt in the
                             // future, therefore ensure the granularity control
                             // by assign to aug_flag
                tags[idx].second = BUCKET_NUM + 2;
                if (!tags[idx].first->is_leaf) {
                    // assert(static_cast<interior*>(tags[idx].first)->aug_flag
                    // ==
                    //        false);
                    static_cast<interior*>(tags[idx].first)->aug_flag =
                        tags[idx].first->size > SERIAL_BUILD_CUTOFF;
                }
            } else {
                tags[idx].second = BUCKET_NUM + 1;
            }
            boxs[tagsNum] = bx;
            rev_tag[tagsNum++] = idx;
            return;
        }

        assert(tags[idx].second == BUCKET_NUM && (!tags[idx].first->is_leaf));
        interior* TI = static_cast<interior*>(tags[idx].first);
        if (hasTomb && (inbalance_node(TI->left->size - sums_tree[idx << 1],
                                       TI->size - sums_tree[idx]))) {
            // WARN: this removes thin leaves
            // if (hasTomb && (inbalance_node(TI->left->size - sums_tree[idx <<
            // 1],
            //                                TI->size - sums_tree[idx]) ||
            //                 (TI->size - sums_tree[idx] < THIN_LEAVE_WRAP))) {
            assert(hasTomb != 0);
            assert(TI->aug_flag == 0);
            tags[idx].second = BUCKET_NUM + 3;
            hasTomb = false;
        }

        // NOTE: hasTomb == false => need to rebuild
        TI->aug_flag = hasTomb ? false : TI->size > SERIAL_BUILD_CUTOFF;

        box lbox(bx), rbox(bx);
        lbox.second.pnt[TI->split.second] = TI->split.first;  //* loose
        rbox.first.pnt[TI->split.second] = TI->split.first;

        mark_tomb(idx << 1, boxs, lbox, hasTomb);
        mark_tomb(idx << 1 | 1, boxs, rbox, hasTomb);
        return;
    }

    void tag_inbalance_node_deletion(box_s& boxs, box bx, bool hasTomb) {
        reduce_sums(1);
        reset_tags_num();
        mark_tomb(1, boxs, bx, hasTomb);
        assert(assert_size(tags[1].first));
        return;
    }

    void init() {
        reset_tags_num();
        tags = node_tags::uninitialized(PIVOT_NUM + BUCKET_NUM + 1);
        sums_tree = parlay::sequence<balls_type>(PIVOT_NUM + BUCKET_NUM + 1);
        rev_tag = tag_nodes::uninitialized(BUCKET_NUM);
    }

    //@ variables
    node_tags tags;
    parlay::sequence<balls_type> sums;
    mutable parlay::sequence<balls_type> sums_tree;
    mutable tag_nodes rev_tag;
    bucket_type tagsNum;
};
}  // namespace cpdd
