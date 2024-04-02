#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

template<typename point>
template<typename Slice>
void ParallelKDtree<point>::flatten(typename ParallelKDtree<point>::node* T,
                                    Slice Out, bool granularity) {
  assert(T->size == Out.size());
  if (T->size == 0) return;

  if (T->is_leaf) {
    leaf* TL = static_cast<leaf*>(T);
    for (int i = 0; i < TL->size; i++) {
      Out[i] = TL->pts[(!T->is_dummy) * i];
    }
    return;
  }

  interior* TI = static_cast<interior*>(T);
  assert(TI->size == TI->left->size + TI->right->size);
  parlay::par_do_if(
      // PERF: check parallelisim using node size can be biased
      (granularity && TI->size > SERIAL_BUILD_CUTOFF) || !granularity,
      [&]() { flatten(TI->left, Out.cut(0, TI->left->size), granularity); },
      [&]() {
        flatten(TI->right, Out.cut(TI->left->size, TI->size), granularity);
      });

  return;
}

template<typename point>
void ParallelKDtree<point>::flatten_and_delete(
    typename ParallelKDtree<point>::node* T, slice Out) {
  assert(T->size == Out.size());
  if (T->is_leaf) {
    leaf* TL = static_cast<leaf*>(T);
    for (int i = 0; i < TL->size; i++) {
      Out[i] = TL->pts[(!T->is_dummy) * i];
    }
    free_leaf(T);
    return;
  }

  interior* TI = static_cast<interior*>(T);
  assert(TI->size == TI->left->size + TI->right->size);
  parlay::par_do_if(
      TI->size > SERIAL_BUILD_CUTOFF,
      [&]() { flatten(TI->left, Out.cut(0, TI->left->size)); },
      [&]() {
        flatten(TI->right, Out.cut(TI->size - TI->right->size, TI->size));
      });
  free_interior(T);

  return;
}

template<typename point>
inline void ParallelKDtree<point>::update_interior(
    typename ParallelKDtree<point>::node* T,
    typename ParallelKDtree<point>::node* L,
    typename ParallelKDtree<point>::node* R) {
  assert(!T->is_leaf);
  interior* TI = static_cast<interior*>(T);
  TI->size = L->size + R->size;
  TI->left = L;
  TI->right = R;
  return;
}

template<typename point>
uint_fast8_t ParallelKDtree<point>::retrive_tag(const point& p,
                                                const node_tags& tags) {
  uint_fast8_t k = 1;
  interior* TI;
  while (k <= PIVOT_NUM && (!tags[k].first->is_leaf)) {
    TI = static_cast<interior*>(tags[k].first);
    k = Num::Lt(p.pnt[TI->split.second], TI->split.first) ? k << 1 : k << 1 | 1;
  }
  assert(tags[k].second < BUCKET_NUM);
  return tags[k].second;
}

template<typename point>
void ParallelKDtree<point>::seieve_points(slice A, slice B, const size_t n,
                                          const node_tags& tags,
                                          parlay::sequence<balls_type>& sums,
                                          const bucket_type tagsNum) {
  size_t num_block = (n + BLOCK_SIZE - 1) >> LOG2_BASE;
  parlay::sequence<parlay::sequence<balls_type>> offset(
      num_block, parlay::sequence<balls_type>(tagsNum));
  assert(offset.size() == num_block && offset[0].size() == tagsNum &&
         offset[0][0] == 0);
  parlay::parallel_for(0, num_block, [&](size_t i) {
    for (size_t j = i << LOG2_BASE; j < std::min((i + 1) << LOG2_BASE, n);
         j++) {
      offset[i][std::move(retrive_tag(A[j], tags))]++;
    }
  });

  sums = parlay::sequence<balls_type>(tagsNum);
  for (size_t i = 0; i < num_block; i++) {
    auto t = offset[i];
    offset[i] = sums;
    for (int j = 0; j < tagsNum; j++) {
      sums[j] += t[j];
    }
  }

  parlay::parallel_for(0, num_block, [&](size_t i) {
    auto v = parlay::sequence<balls_type>::uninitialized(tagsNum);
    int tot = 0, s_offset = 0;
    for (int k = 0; k < tagsNum - 1; k++) {
      v[k] = tot + offset[i][k];
      tot += sums[k];
      s_offset += offset[i][k];
    }
    v[tagsNum - 1] = tot + ((i << LOG2_BASE) - s_offset);
    for (size_t j = i << LOG2_BASE; j < std::min((i + 1) << LOG2_BASE, n);
         j++) {
      B[v[std::move(retrive_tag(A[j], tags))]++] = A[j];
    }
  });

  return;
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::update_inner_tree(bucket_type idx, const node_tags& tags,
                                         parlay::sequence<node_box>& treeNodes,
                                         bucket_type& p,
                                         const tag_nodes& rev_tag) {
  if (tags[idx].second < BUCKET_NUM) {
    assert(rev_tag[p] == idx);
    return treeNodes[p++];
  }

  assert(tags[idx].second == BUCKET_NUM);
  assert(tags[idx].first != nullptr);
  auto [L, Lbox] = update_inner_tree(idx << 1, tags, treeNodes, p, rev_tag);
  auto [R, Rbox] = update_inner_tree(idx << 1 | 1, tags, treeNodes, p, rev_tag);
  update_interior(tags[idx].first, L, R);
  return node_box(tags[idx].first, get_box(Lbox, Rbox));
}

// NOTE: rebuild a single (sub)-tree
template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::rebuild_single_tree(node* T, const dim_type d,
                                           const dim_type DIM,
                                           const bool granularity) {
  points wo = points::uninitialized(T->size);
  points wx = points::uninitialized(T->size);
  uint_fast8_t curDim = pick_rebuild_dim(T, d, DIM);
  flatten(T, wx.cut(0, T->size), granularity);
  delete_tree_recursive(T, granularity);
  box bx = get_box(parlay::make_slice(wx));
  node* o = build_recursive(parlay::make_slice(wx), parlay::make_slice(wo),
                            curDim, DIM, bx);
  return node_box(std::move(o), std::move(bx));
}

template<typename point>
typename ParallelKDtree<point>::node_box
ParallelKDtree<point>::rebuild_tree_recursive(node* T, dim_type d,
                                              const dim_type DIM,
                                              const bool granularity) {
  if (T->is_leaf) {
    return node_box(T, get_box(T));
  }

  interior* TI = static_cast<interior*>(T);
  if (inbalance_node(TI->left->size, TI->size)) {
    return rebuild_single_tree(T, d, DIM, granularity);
  }

  node *L, *R;
  box Lbox, Rbox;
  d = (d + 1) % DIM;
  parlay::par_do_if(
      // NOTE: if granularity is disabled, always traverse the tree in
      // parallel
      (granularity && T->size > SERIAL_BUILD_CUTOFF) || !granularity,
      [&] {
        std::tie(L, Lbox) =
            rebuild_tree_recursive(TI->left, d, DIM, granularity);
      },
      [&] {
        std::tie(R, Rbox) =
            rebuild_tree_recursive(TI->right, d, DIM, granularity);
      });

  update_interior(T, L, R);

  return node_box(T, get_box(Lbox, Rbox));
}

template<typename point>
typename ParallelKDtree<point>::node* ParallelKDtree<point>::delete_tree() {
  if (this->root == nullptr) {
    return this->root;
  }
  delete_tree_recursive(this->root);
  this->root = nullptr;
  return this->root;
}

template<typename point>  // NOTE: delete tree in parallel
void ParallelKDtree<point>::delete_tree_recursive(node* T, bool granularity) {
  if (T == nullptr) return;
  if (T->is_leaf) {
    free_leaf(T);
  } else {
    interior* TI = static_cast<interior*>(T);

    // NOTE: enable granularity control by default, if it is disabled,
    // always delete in parallel
    parlay::par_do_if(
        (granularity && T->size > SERIAL_BUILD_CUTOFF) || !granularity,
        [&] { delete_tree_recursive(TI->left, granularity); },
        [&] { delete_tree_recursive(TI->right, granularity); });
    free_interior(T);
  }
}

template<typename point>  // NOTE: (minor) delete simple tree in parallel
void ParallelKDtree<point>::delete_simple_tree_recursive(simple_node* T) {
  if (T == nullptr) return;
  parlay::par_do_if(
      0, [&] { delete_simple_tree_recursive(T->left); },
      [&] { delete_simple_tree_recursive(T->right); });
  free_simple_node(T);
}

}  // namespace cpdd
