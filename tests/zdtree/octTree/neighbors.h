// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "parlay/internal/group_by.h"
bool report_stats = true;
int algorithm_version = 0;
// 0=root based, 1=bit based, >2=map based

#include <math.h>

#include <algorithm>
#include <queue>

#include "common/geometry.h"
#include "common/geometryIO.h"
#include "common/time_loop.h"
#include "k_nearest_neighbors.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/random.h"

static constexpr double batchQueryRatio = 0.01;
static constexpr int rangeQueryNum = 100;
// static constexpr double batchInsertRatio = 0.001;
static constexpr double batchInsertRatio = 0.01;

template<class Point>
parlay::sequence<Point> readGeneral(char const *fname) {
    parlay::sequence<char> S = readStringFromFile(fname);
    parlay::sequence<char *> W = stringToWords(S);
    int d = Point::dim;
    //  std::cout << W.size() << std::endl;
    return parsePoints<Point>(W.cut(2, W.size()));
}

//* export LD_PRELOAD=/usr/local/lib/libjemalloc.so.2 *//

// find the k nearest neighbors for all points in tree
// places pointers to them in the .ngh field of each vertex
template<int max_k, class vpoint, class vtx>
void ANN(parlay::sequence<vtx> &v, int k, int rounds, parlay::sequence<vtx> &vin, int tag, int queryType,
         const int summary) {
    //  timer t( "ANN", report_stats );

    {
        // timer _t;
        // _t.start();
        using knn_tree = k_nearest_neighbors<vtx, max_k>;
        using node = typename knn_tree::node;
        using box = typename knn_tree::box;
        using box_delta = std::pair<box, double>;

        // create sequences for insertion and deletion
        size_t n = v.size();

        parlay::sequence<vtx *> v2, vin2, allv;

        v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
        box whole_box = knn_tree::o_tree::get_box(v2);
        knn_tree T = knn_tree(v2, whole_box);

        //* build
        double aveBuild = time_loop(
            rounds, 1.0, [&]() {},
            [&]() {
                v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                whole_box = knn_tree::o_tree::get_box(v2);
                T = knn_tree(v2, whole_box);
            },
            [&]() { T.tree.reset(); });

        v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
        whole_box = knn_tree::o_tree::get_box(v2);
        T = knn_tree(v2, whole_box);

        std::cout << aveBuild << " " << T.tree.get()->depth() << " " << std::flush;

        int dims;
        node *root;
        box_delta bd;

        //* batch-dynamic insertion
        if (tag >= 1) {
            auto zdtree_batch_insert = [&](double r) {
                size_t sz;
                sz = static_cast<size_t>(vin.size() * r);

                double aveInsert = time_loop(
                    rounds, 1.0,
                    [&]() {
                        T.tree.reset();
                        v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                        vin2 = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &vin[i]; });
                        allv = parlay::append(v2, vin2);
                        whole_box = knn_tree::o_tree::get_box(allv);
                        T = knn_tree(v2, whole_box);
                    },
                    [&]() {
                        vin2 = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &vin[i]; });
                        dims = vin2[0]->pt.dimension();
                        root = T.tree.get();
                        bd = T.get_box_delta(dims);

                        T.batch_insert(vin2, root, bd.first, bd.second);
                    },
                    [&]() { T.tree.reset(); });
                std::cout << aveInsert << " " << std::flush;

                //* restore
                // T.tree.reset();
                // v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                // vin2 = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &vin[i];
                // }); allv = parlay::append(v2, vin2); whole_box =
                // knn_tree::o_tree::get_box(allv); T = knn_tree(v2, whole_box); dims =
                // vin2[0]->pt.dimension(); root = T.tree.get(); bd =
                // T.get_box_delta(dims); T.batch_insert(vin2, root, bd.first,
                // bd.second);
                //! no need to append vin since KNN graph always get points from the
                //! tree
            };

            if (summary) {
                const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
                for (int i = 0; i < ratios.size(); i++) {
                    zdtree_batch_insert(ratios[i]);
                }
            } else {
                zdtree_batch_insert(batchInsertRatio);
            }
        }

        if (tag >= 2) {
            auto zdtree_batch_delete = [&](double r) {
                size_t sz;
                sz = static_cast<size_t>(vin.size() * r);
                double aveDelete = time_loop(
                    rounds, 1.0,
                    [&]() {
                        T.tree.reset();
                        v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                        whole_box = knn_tree::o_tree::get_box(v2);
                        T = knn_tree(v2, whole_box);

                        dims = v2[0]->pt.dimension();
                        root = T.tree.get();
                        bd = T.get_box_delta(dims);
                    },
                    [&]() {
                        v2 = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &v[i]; });
                        T.batch_delete(v2, root, bd.first, bd.second);
                    },
                    [&]() { T.tree.reset(); });
                std::cout << aveDelete << " " << std::flush;

                T.tree.reset();
                v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                whole_box = knn_tree::o_tree::get_box(v2);
                T = knn_tree(v2, whole_box);
            };

            if (summary) {
                const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
                for (int i = 0; i < ratios.size(); i++) {
                    zdtree_batch_delete(ratios[i]);
                }
            } else {
                zdtree_batch_delete(batchInsertRatio);
            }
        }

        parlay::sequence<size_t> visNodeNum(n, 0);
        if (queryType & (1 << 0)) {  //* KNN
            auto run_zdtree_knn = [&](int kth, size_t batchSize) {
                parlay::sequence<vtx *> vr = parlay::tabulate(batchSize, [&](size_t i) -> vtx * { return &v[i]; });
                auto aveQuery = time_loop(
                    rounds, 1.0, [&]() {},
                    [&]() {
                        if (algorithm_version == 0) {
                            // parlay::sequence<vtx *> vr = T.vertices();
                            size_t n = vr.size();
                            parlay::parallel_for(0, n, [&](size_t i) {
                                T.k_nearest(vr[i], kth);
                                visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                            });
                        } else if (algorithm_version == 1) {
                        } else {
                        }
                    },
                    [&]() {});
                std::cout << aveQuery << " " << T.tree.get()->depth() << " " << parlay::reduce(visNodeNum) / batchSize
                          << " " << std::flush;
            };

            size_t batchSize = static_cast<size_t>(v.size() * batchQueryRatio);
            if (summary == 0) {
                int K[3] = {1, 10, 100};
                for (int i = 0; i < 3; i++) {
                    run_zdtree_knn(K[i], batchSize);
                }
            } else {
                run_zdtree_knn(k, batchSize);
            }
        }

        if (queryType & (1 << 1)) {  // NOTE: batch query
            auto run_batch_knn = [&](int kth, parlay::sequence<vtx> &pts, double ratios) {
                size_t sz = static_cast<size_t>(v.size() * ratios);
                parlay::sequence<vtx *> vr = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &pts[i]; });
                auto aveQuery = time_loop(
                    rounds, 1.0, [&]() {},
                    [&]() {
                        if (algorithm_version == 0) {
                            size_t n = vr.size();
                            parlay::parallel_for(0, n, [&](size_t i) {
                                T.k_nearest(vr[i], kth);
                                visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                            });
                        } else if (algorithm_version == 1) {
                        } else {
                        }
                    },
                    [&]() {});
                std::cout << aveQuery << " " << T.tree.get()->depth() << " "
                          << parlay::reduce(visNodeNum) / T.tree.get()->size() << " " << std::flush;
                // std::cout << aveQuery << " " << std::flush;
            };

            run_batch_knn(k, v, batchQueryRatio);

            // std::vector<double> ratios = {0.001, 0.01, 0.1, 0.2, 0.5};
            // for (auto r : ratios) {
            //   run_batch_knn(k, v, r);
            // }
            // for (auto r : ratios) {
            //   run_batch_knn(k, vin, r);
            // }
        }

        // TODO rewrite range count

        // auto boxs = T.gen_rectangles( 1000, 2, v, ( v[0].pt ).dimension() );
        // for( int i = 0; i < 10; i++ ) {
        //   boxs[i].first.print();
        //   boxs[i].second.print();
        //   LOG << ENDL;
        // }
        // return;

        int queryNum = 100;
        if (queryType & (1 << 2)) {  //* range Count
            double aveQuery = time_loop(
                rounds, 1.0, [&]() {},
                [&]() {
                    // for( int i = 0; i < 10; i++ ) {
                    //   parlay::sequence<vtx*> pts{ v[i], v[( i + n / 2 ) % size] };
                    //   box queryBox = knn_tree::o_tree::get_box( pts );
                    //   int dims = ( v[0]->pt ).dimension();
                    //   box_delta bd = T.get_box_delta( dims );
                    //   T.range_count( T.tree.get(), queryBox, 1e-7 );
                    //   //  std::cout << T.tree.get()->get_aug() << std::endl;
                    // }
                },
                [&]() {});
            std::cout << "-1 -1 -1 " << std::flush;
        }

        if (queryType & (1 << 3)) {  //* range query
            // parlay::sequence<vtx*> Out( size );
            // double aveQuery = time_loop(
            //     rounds, 1.0, [&]() {},
            //     [&]() {
            //       for( int i = 0; i < 10; i++ ) {
            //         parlay::sequence<vtx*> pts{ v[i], v[( i + size / 2 ) % size]
            //         }; box queryBox = knn_tree::o_tree::get_box( pts ); int dims
            //         = ( v[0]->pt ).dimension(); box_delta bd = T.get_box_delta(
            //         dims ); T.range_count( T.tree.get(), queryBox, 1e-7 );
            //         T.range_query( T.tree.get(),
            //                        Out.cut( 0, T.tree.get()->get_aug() ),
            //                        queryBox, 1e-7 );
            //         //  std::cout << T.tree.get()->get_aug() << std::endl;
            //       }
            //     },
            //     [&]() {} );
            if (summary == 0) {
                std::cout << "-1 -1 -1 " << std::flush;
            } else {
                std::cout << "-1 " << std::flush;
            }
        }

        if (queryType & (1 << 4)) {  // NOTE: batch insertion with fraction
            const parlay::sequence<double> ratios = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                                                     0.02,   0.05,   0.1,    0.2,   0.5,   1.0};
            for (int i = 0; i < ratios.size(); i++) {
                size_t sz = size_t(v.size() * ratios[i]);

                double aveInsert = time_loop(
                    rounds, 1.0,
                    [&]() {
                        T.tree.reset();
                        v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                        vin2 = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &vin[i]; });
                        allv = parlay::append(v2, vin2);
                        whole_box = knn_tree::o_tree::get_box(allv);
                        T = knn_tree(v2, whole_box);
                    },
                    [&]() {
                        dims = vin2[0]->pt.dimension();
                        root = T.tree.get();
                        bd = T.get_box_delta(dims);

                        T.batch_insert(vin2, root, bd.first, bd.second);
                    },
                    [&]() { T.tree.reset(); });

                std::cout << aveInsert << " " << std::flush;
            }
        }

        if (queryType & (1 << 5)) {  // NOTE: batch deletion with fraction
            const parlay::sequence<double> ratios = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                                                     0.02,   0.05,   0.1,    0.2,   0.5,   1.0};
            for (int i = 0; i < ratios.size(); i++) {
                size_t sz = size_t(v.size() * ratios[i]);
                if (i == ratios.size() - 1) sz = v.size() - 100;  // NOTE: bug in their implementation

                double aveDelete = time_loop(
                    rounds, 1.0,
                    [&]() {
                        T.tree.reset();
                        v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
                        whole_box = knn_tree::o_tree::get_box(v2);
                        T = knn_tree(v2, whole_box);

                        dims = v2[0]->pt.dimension();
                        root = T.tree.get();
                        bd = T.get_box_delta(dims);
                    },
                    [&]() {
                        v2 = parlay::tabulate(sz, [&](size_t i) -> vtx * { return &v[i]; });
                        T.batch_delete(v2, root, bd.first, bd.second);
                    },
                    [&]() { T.tree.reset(); });
                std::cout << aveDelete << " " << std::flush;
            }
        }

        if (queryType & (1 << 8)) {  //* batch insertion then knn
            //* first run general build
            T.tree.reset();
            v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
            whole_box = knn_tree::o_tree::get_box(v2);
            T = knn_tree(v2, whole_box);

            auto aveQuery = time_loop(
                rounds, 1.0, [&]() {},
                [&]() {
                    if (algorithm_version == 0) {  // this is for starting from
                        parlay::sequence<vtx *> vr = T.vertices();
                        // find nearest k neighbors for each point
                        //  vr = T.vertices();
                        //  vr = parlay::random_shuffle( vr.cut( 0, vr.size() ) );
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest(vr[i], k);
                            visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                        });
                    } else if (algorithm_version == 1) {
                        parlay::sequence<vtx *> vr = T.vertices();
                        int dims = (v[0].pt).dimension();
                        node *root = T.tree.get();
                        box_delta bd = T.get_box_delta(dims);
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest_leaf(vr[i], T.find_leaf(vr[i]->pt, root, bd.first, bd.second), k);
                        });
                    } else {
                        auto f = [&](vtx *p, node *n) { return T.k_nearest_leaf(p, n, k); };
                        // find nearest k neighbors for each point
                        T.tree->map(f);
                    }
                },
                [&]() {});
            std::cout << aveQuery << " " << T.tree.get()->depth() << " "
                      << parlay::reduce(visNodeNum) / T.tree.get()->size() << " " << std::flush;

            //* then incremental build
            T.tree.reset();
            v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
            vin2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &vin[i]; });
            allv = parlay::append(v2, vin2);
            whole_box = knn_tree::o_tree::get_box(allv);

            size_t sz = n * 0.1;
            vin2.resize(sz);
            parlay::parallel_for(0, sz, [&](size_t i) { vin2[i] = &v[i]; });
            T = knn_tree(vin2, whole_box);

            root = T.tree.get();
            // LOG << n << " " << root->size() << ENDL;

            size_t l = sz, r = 0;
            while (l < n) {
                r = std::min(l + sz, n);
                dims = vin2[0]->pt.dimension();
                root = T.tree.get();
                bd = T.get_box_delta(dims);
                vin2.resize(r - l);
                parlay::parallel_for(0, r - l, [&](size_t i) { vin2[i] = &vin[l + i]; });
                T.batch_insert(vin2, root, bd.first, bd.second);

                l = r;
            }

            // LOG << root->depth() << ENDL;

            aveQuery = time_loop(
                rounds, 1.0, [&]() {},
                [&]() {
                    if (algorithm_version == 0) {  // this is for starting from
                        parlay::sequence<vtx *> vr = T.vertices();
                        // find nearest k neighbors for each point
                        //  vr = T.vertices();
                        //  vr = parlay::random_shuffle( vr.cut( 0, vr.size() ) );
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest(vr[i], k);
                            visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                        });
                    } else if (algorithm_version == 1) {
                        parlay::sequence<vtx *> vr = T.vertices();
                        int dims = (v[0].pt).dimension();
                        node *root = T.tree.get();
                        box_delta bd = T.get_box_delta(dims);
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest_leaf(vr[i], T.find_leaf(vr[i]->pt, root, bd.first, bd.second), k);
                        });
                    } else {
                        auto f = [&](vtx *p, node *n) { return T.k_nearest_leaf(p, n, k); };
                        // find nearest k neighbors for each point
                        T.tree->map(f);
                    }
                },
                [&]() {});
            std::cout << aveQuery << " " << T.tree.get()->depth() << " "
                      << parlay::reduce(visNodeNum) / T.tree.get()->size() << " " << std::flush;
        }

        if (queryType & (1 << 9)) {  //* batch deletion then knn
                                     //* first run general build
            T.tree.reset();
            v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
            whole_box = knn_tree::o_tree::get_box(v2);
            T = knn_tree(v2, whole_box);

            auto aveQuery = time_loop(
                rounds, 1.0, [&]() {},
                [&]() {
                    k = 100;
                    if (algorithm_version == 0) {  // this is for starting from
                        parlay::sequence<vtx *> vr = T.vertices();
                        // find nearest k neighbors for each point
                        //  vr = T.vertices();
                        //  vr = parlay::random_shuffle( vr.cut( 0, vr.size() ) );
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest(vr[i], k);
                            visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                        });
                    } else if (algorithm_version == 1) {
                    } else {
                    }
                },
                [&]() {});
            std::cout << aveQuery << " " << T.tree.get()->depth() << " "
                      << parlay::reduce(visNodeNum) / T.tree.get()->size() << " " << std::flush;
            //* then incremental delete
            T.tree.reset();
            v2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &v[i]; });
            vin2 = parlay::tabulate(n, [&](size_t i) -> vtx * { return &vin[i]; });
            allv = parlay::append(v2, vin2);
            whole_box = knn_tree::o_tree::get_box(allv);
            T = knn_tree(allv, whole_box);

            size_t sz = n * 0.1;

            size_t l = sz, r = 0;
            while (l < n - 100) {
                r = std::min(l + sz, n - 100);
                dims = vin2[0]->pt.dimension();
                root = T.tree.get();
                bd = T.get_box_delta(dims);
                vin2.resize(r - l);
                parlay::parallel_for(0, r - l, [&](size_t i) { vin2[i] = &vin[l + i]; });
                T.batch_delete(vin2, root, bd.first, bd.second);
                l = r;
            }

            aveQuery = time_loop(
                rounds, 1.0, [&]() {},
                [&]() {
                    k = 100;
                    if (algorithm_version == 0) {  // this is for starting from
                        parlay::sequence<vtx *> vr = T.vertices();
                        // find nearest k neighbors for each point
                        //  vr = T.vertices();
                        //  vr = parlay::random_shuffle( vr.cut( 0, vr.size() ) );
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest(vr[i], k);
                            visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                        });
                    } else if (algorithm_version == 1) {
                    } else {
                    }
                },
                [&]() {});
            std::cout << aveQuery << " " << T.tree.get()->depth() << " "
                      << parlay::reduce(visNodeNum) / T.tree.get()->size() << " " << std::flush;
        }

        auto insertOsmByTime = [&](parlay::sequence<parlay::sequence<vtx>> &node_by_time,
                                   parlay::sequence<vtx *> &all_points, int time_period_num) {
            const size_t osm_query_num = 100000;
            parlay::sequence<parlay::sequence<vtx *>> wp(time_period_num);

            wp[0] = parlay::tabulate(node_by_time[0].size(), [&](size_t j) -> vtx * { return &node_by_time[0][j]; });
            auto whole_box = knn_tree::o_tree::get_box(all_points);
            T = knn_tree(wp[0], whole_box);

            // double aveInsert = time_loop(
            //     rounds, 1.0, [&]() { T.tree.reset(); },
            //     [&]() {
            //         wp[0] = parlay::tabulate(node_by_time[0].size(),
            //                                  [&](size_t j) -> vtx * { return &node_by_time[0][j]; });
            //         auto whole_box = knn_tree::o_tree::get_box(all_points);
            //         T = knn_tree(wp[0], whole_box);
            //         for (int i = 1; i < time_period_num; i++) {
            //             wp[i] = parlay::tabulate(node_by_time[i].size(),
            //                                      [&](size_t j) -> vtx * { return &node_by_time[i][j]; });
            //             dims = wp[i][0]->pt.dimension();
            //             root = T.tree.get();
            //             bd = T.get_box_delta(dims);
            //             T.batch_insert(wp[i], root, bd.first, bd.second);
            //         }
            //     },
            //     [&]() { T.tree.reset(); });

            parlay::internal::timer t;
            T.tree.reset();
            t.reset(), t.start();
            // wp[0] = parlay::tabulate(node_by_time[0].size(), [&](size_t j) -> vtx * { return &node_by_time[0][j]; });
            wp[0] = parlay::tabulate(node_by_time[0].size(), [&](size_t i) { return all_points[i]; });
            auto wbox = knn_tree::o_tree::get_box(all_points);
            T = knn_tree(wp[0], wbox);
            t.stop();

            auto verifyZdtreeDepthSize = [&]() {
                size_t idx = 0;
                parlay::sequence<int> size_arr(T.tree.get()->size(), 0);
                parlay::sequence<int> depth_arr(T.tree.get()->size(), 0);
                T.tree.get()->get_leaf_info(idx, T.tree.get(), 1, size_arr, depth_arr);
                auto size_histo = parlay::histogram_by_key(size_arr.cut(0, idx));
                auto depth_histo = parlay::histogram_by_key(depth_arr.cut(0, idx));
                parlay::sort_inplace(size_histo, [&](auto &a, auto &b) { return a.first < b.first; });
                parlay::sort_inplace(depth_histo, [&](auto &a, auto &b) { return a.first < b.first; });
                LOG << wp[0].size() << " " << t.total_time() << ENDL;
                LOG << "tree size: " << ENDL;
                for (auto i : size_histo) {
                    LOG << i.first << " " << i.second << ENDL;
                }
                LOG << "tree depth: " << ENDL;
                for (auto i : depth_histo) {
                    LOG << i.first << " " << i.second << ENDL;
                }
                LOG << "--------------------" << ENDL;
            };

            // NOTE: test the knn time
            auto zdtreeKNN = [&](const int kth, auto pts) {
                LOG << ">>> knn: ";
                // BUG: 100724349
                auto aveQuery = time_loop(
                    rounds, -1.0, [&]() {},  // NOTE: too long, only run once
                    [&]() {
                        if (algorithm_version == 0) {
                            // parlay::sequence<vtx *> vr = T.vertices();
                            // LOG << k << " " << vr.size() << ENDL;
                            size_t n = pts.size();
                            parlay::parallel_for(0, n, [&](size_t i) {
                                // for (int i = 100724349; i < n; i++) {
                                // if (i % 1000000 == 0) LOG << i << ENDL;
                                T.k_nearest(pts[i], kth);
                                visNodeNum[i] = pts[i]->counter + pts[i]->counter2;
                                // }
                            });
                        } else if (algorithm_version == 1) {
                        } else {
                        }
                    },
                    [&]() {});
                std::cout << aveQuery << " " << T.tree.get()->depth() << " " << parlay::reduce(visNodeNum) / pts.size()
                          << " " << std::endl
                          << std::flush;
            };

            // verifyZdtreeInfo();
            size_t cnt = wp[0].size();
            // zdtreeKNN(10, all_points.cut(0, osm_query_num));
            T.verifyBoundindBox(T.tree.get());
            LOG << "here" << ENDL;

            for (int i = 1; i < time_period_num; i++) {
                t.reset(), t.start();
                wp[i] =
                    parlay::tabulate(node_by_time[i].size(), [&](size_t j) -> vtx * { return &node_by_time[i][j]; });
                dims = wp[i][0]->pt.dimension();
                root = T.tree.get();
                bd = T.get_box_delta(dims);
                T.batch_insert(wp[i], root, bd.first, bd.second);
                t.stop();
                LOG << node_by_time[i].size() << " " << t.total_time() << ENDL;

                // NOTE: test the knn time
                // verifyZdtreeInfo();
                cnt += wp[i].size();
                T.verifyBoundindBox(T.tree.get());
                // zdtreeKNN(10, all_points.cut(0, osm_query_num));
            }

            // std::cout << aveInsert << " " << T.tree.get()->depth() << " " << "-1 " << std::flush;
        };

        if (queryType & (1 << 11)) {  // NOTE: by year
            LOG << ENDL;
            string osm_prefix = "/data/zmen002/kdtree/real_world/osm/year/";
            const std::vector<std::string> files = {"2014", "2015", "2016", "2017", "2018",
                                                    "2019", "2020", "2021", "2022", "2023"};
            parlay::sequence<parlay::sequence<vtx>> node_by_year(files.size());
            parlay::sequence<parlay::sequence<vtx *>> star_node_by_year(files.size());
            for (int i = 0; i < files.size(); i++) {
                std::string path = osm_prefix + "osm_" + files[i] + ".csv";
                auto pts = readGeneral<vpoint>(path.c_str());
                node_by_year[i] = parlay::tabulate(pts.size(), [&](size_t j) -> vtx { return vtx(pts[j], j); });
                star_node_by_year[i] =
                    parlay::tabulate(pts.size(), [&](size_t j) -> vtx * { return &node_by_year[i][j]; });
            }

            auto star_all = parlay::flatten(star_node_by_year);
            insertOsmByTime(node_by_year, star_all, node_by_year.size());

            // NOTE: begin knn
            auto aveQuery = time_loop(
                rounds, -1.0, [&]() {},  // NOTE: too long, only run once
                [&]() {
                    if (algorithm_version == 0) {
                        // parlay::sequence<vtx *> vr = T.vertices();
                        auto vr = star_all;  // WARN: cannot move it outside otherwise segfault
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest(vr[i], k);
                            visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                        });
                    } else if (algorithm_version == 1) {
                    } else {
                    }
                },
                [&]() {});
            std::cout << aveQuery << " " << T.tree.get()->depth() << " " << parlay::reduce(visNodeNum) / star_all.size()
                      << " " << std::flush;
        }

        if (queryType & (1 << 12)) {  // NOTE: by month
            LOG << ENDL;
            string osm_prefix = "/data/zmen002/kdtree/real_world/osm/month/";
            const std::vector<std::string> files = {"2014", "2015", "2016", "2017", "2018",
                                                    "2019", "2020", "2021", "2022", "2023"};
            const std::vector<std::string> month = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};

            parlay::sequence<parlay::sequence<vtx>> node_by_year(files.size() * month.size());  // NOTE: point
            parlay::sequence<parlay::sequence<vtx *>> star_node_by_year(node_by_year.size());   // NOTE: their address

            for (int i = 0; i < files.size(); i++) {
                for (int k = 0; k < month.size(); k++) {
                    std::string path = osm_prefix + files[i] + "/" + month[k] + ".csv";
                    auto pts = readGeneral<vpoint>(path.c_str());
                    int month_pos = i * month.size() + k;
                    node_by_year[month_pos] =
                        parlay::tabulate(pts.size(), [&](size_t j) -> vtx { return vtx(pts[j], j); });
                    star_node_by_year[month_pos] =
                        parlay::tabulate(pts.size(), [&](size_t j) -> vtx * { return &node_by_year[month_pos][j]; });
                }
            }

            auto star_all = parlay::flatten(star_node_by_year);
            insertOsmByTime(node_by_year, star_all, node_by_year.size());

            // NOTE: begin knn
            auto aveQuery = time_loop(
                rounds, -1.0, [&]() {},  // NOTE: too long, only run once
                [&]() {
                    if (algorithm_version == 0) {
                        // parlay::sequence<vtx *> vr = T.vertices();
                        auto vr = star_all;  // WARN: cannot move it outside otherwise segfault
                        size_t n = vr.size();
                        parlay::parallel_for(0, n, [&](size_t i) {
                            T.k_nearest(vr[i], k);
                            visNodeNum[i] = vr[i]->counter + vr[i]->counter2;
                        });
                    } else if (algorithm_version == 1) {
                    } else {
                    }
                },
                [&]() {});
            std::cout << aveQuery << " " << T.tree.get()->depth() << " " << parlay::reduce(visNodeNum) / star_all.size()
                      << " " << std::flush;
        }

        std::cout << std::endl << std::flush;
    };
}
