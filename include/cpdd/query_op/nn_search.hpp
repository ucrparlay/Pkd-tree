#pragma once

#include "../kdTreeParallel.h"

namespace cpdd {

//* NN search
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::p2p_distance(const point& p, const point& q,
                                    const dim_type DIM) {
  coord r = 0;
  for (dim_type i = 0; i < DIM; i++) {
    r += (p.pnt[i] - q.pnt[i]) * (p.pnt[i] - q.pnt[i]);
  }
  return std::move(r);
}

//* distance between a point and a box
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::p2b_min_distance(
    const point& p, const typename ParallelKDtree<point>::box& a,
    const dim_type DIM) {
  coord r = 0;
  for (dim_type i = 0; i < DIM; i++) {
    if (Num::Lt(p.pnt[i], a.first.pnt[i])) {
      r += (a.first.pnt[i] - p.pnt[i]) * (a.first.pnt[i] - p.pnt[i]);
    } else if (Num::Gt(p.pnt[i], a.second.pnt[i])) {
      r += (p.pnt[i] - a.second.pnt[i]) * (p.pnt[i] - a.second.pnt[i]);
    }
  }
  return r;
}

template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::p2b_max_distance(
    const point& p, const typename ParallelKDtree<point>::box& a,
    const dim_type DIM) {
  coord r = 0;
  for (dim_type i = 0; i < DIM; i++) {
    if (Num::Lt(p.pnt[i], (a.second.pnt[i] + a.first.pnt[i]) / 2)) {
      r += (a.second.pnt[i] - p.pnt[i]) * (a.second.pnt[i] - p.pnt[i]);
    } else {
      r += (p.pnt[i] - a.first.pnt[i]) * (p.pnt[i] - a.first.pnt[i]);
    }
  }
  return r;
}

//* early return the partial distance between p and q if it is larger than r
//* else return the distance between p and q
template<typename point>
inline typename ParallelKDtree<point>::coord
ParallelKDtree<point>::interruptible_distance(const point& p, const point& q,
                                              coord up, dim_type DIM) {
  coord r = 0;
  dim_type i = 0;
  if (DIM >= 6) {
    while (1) {
      r += (p.pnt[i] - q.pnt[i]) * (p.pnt[i] - q.pnt[i]);
      i++;
      r += (p.pnt[i] - q.pnt[i]) * (p.pnt[i] - q.pnt[i]);
      i++;
      r += (p.pnt[i] - q.pnt[i]) * (p.pnt[i] - q.pnt[i]);
      i++;
      r += (p.pnt[i] - q.pnt[i]) * (p.pnt[i] - q.pnt[i]);
      i++;

      if (Num::Gt(r, up)) {
        return r;
      }
      if (i + 4 > DIM) {
        break;
      }
    }
  }
  while (i < DIM) {
    r += (p.pnt[i] - q.pnt[i]) * (p.pnt[i] - q.pnt[i]);
    i++;
  }
  return r;
}

template<typename point>
template<typename StoreType>
void ParallelKDtree<point>::k_nearest(node* T, const point& q,
                                      const dim_type DIM,
                                      kBoundedQueue<point, StoreType>& bq,
                                      const box& nodeBox, size_t& visNodeNum) {
  visNodeNum++;

  if (T->is_leaf) {
    leaf* TL = static_cast<leaf*>(T);
    int i = 0;
    while (!bq.full() && i < TL->size) {
      bq.insert(
          std::make_pair(std::ref(TL->pts[(!T->is_dummy) * i]),
                         p2p_distance(q, TL->pts[(!T->is_dummy) * i], DIM)));
      i++;
    }
    while (i < TL->size) {
      coord r = interruptible_distance(q, TL->pts[(!T->is_dummy) * i],
                                       bq.top_value(), DIM);
      if (Num::Lt(r, bq.top_value())) {
        bq.insert(std::make_pair(std::ref(TL->pts[(!T->is_dummy) * i]), r));
      } else if (TL->is_dummy) {
        break;
      }
      i++;
    }
    return;
  }

  interior* TI = static_cast<interior*>(T);
  auto go_left = [&]() -> bool {
    return Num::Gt(TI->split.first - q.pnt[TI->split.second], 0);
  };

  box firstBox(nodeBox), secondBox(nodeBox);

  if (go_left()) {  //* go left child
    firstBox.second.pnt[TI->split.second] = TI->split.first;
    secondBox.first.pnt[TI->split.second] = TI->split.first;
  } else {  //* go right child
    firstBox.first.pnt[TI->split.second] = TI->split.first;
    secondBox.second.pnt[TI->split.second] = TI->split.first;
  }

  k_nearest(go_left() ? TI->left : TI->right, q, DIM, bq, firstBox, visNodeNum);
  if (Num::Gt(p2b_min_distance(q, secondBox, DIM), bq.top_value()) &&
      bq.full()) {
    return;
  }
  k_nearest(go_left() ? TI->right : TI->left, q, DIM, bq, secondBox,
            visNodeNum);
  return;
}

}  // namespace cpdd
