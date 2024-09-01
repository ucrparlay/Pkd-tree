#include <iostream>
#include "common/parse_command_line.h"
#include "parlay/internal/get_time.h"
#include "parlay/parallel.h"
#include "parlay/slice.h"
#include "testFramework.h"
#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

using Typename = coord;
// Define a 2D point
typedef bg::model::point<coord, 2, bg::cs::cartesian> RPoint;
// Define a box (for range queries)
typedef bg::model::box<RPoint> RBox;
// Define a Point type that the R-tree will store (a point and its associated data)

template<typename point>
void testRtreeParallel(int Dim, int LEAVE_WRAP, parlay::sequence<point>& wp, int N, int K, const int& rounds,
                       const string& insertFile, const int& tag, const int& queryType, const int summary) {
    using points = parlay::sequence<point>;
    using pkdTree = ParallelKDtree<point>;
    using box = typename pkdTree::box;
    points wi;
    if (insertFile != "") {
        auto [nn, nd] = read_points<point>(insertFile.c_str(), wi, K);
        if (nd != Dim) {
            puts("read inserted points dimension wrong");
            abort();
        }
    }

    // Create an empty R-tree using default parameters

    // Sample points to insert into the R-tree
    std::vector<RPoint> _points(wp.size());
    std::vector<RPoint> _points_insert(wi.size());
    assert(wp.size() == wi.size());
    parlay::parallel_for(0, wi.size(), [&](size_t i) {
        bg::set<0>(_points[i], wp[i].pnt[0]);
        bg::set<0>(_points_insert[i], wi[i].pnt[0]);

        bg::set<1>(_points[i], wp[i].pnt[1]);
        bg::set<1>(_points_insert[i], wi[i].pnt[1]);
    });

    parlay::internal::timer timer;
    timer.start();
    bgi::rtree<RPoint, bgi::quadratic<32>> tree(_points.begin(), _points.end());
    timer.stop();
    std::cout << timer.total_time() << " " << -1 << " " << std::flush;

    if (tag >= 1) {
        auto rtree_insert = [&](double r) {
            timer.reset();
            timer.start();
            size_t sz = _points_insert.size() * r;
            tree.insert(_points_insert.begin(), _points_insert.begin() + sz);
            std::cout << timer.total_time() << " " << std::flush;
        };

        if (summary) {
            const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
            for (int i = 0; i < ratios.size(); i++) {
                tree.clear();
                tree.insert(_points.begin(), _points.end());
                rtree_insert(ratios[i]);
            }
        } else {
            rtree_insert(batchInsertRatio);
        }

        if (tag == 1) wp.append(wi);
    }

    if (tag >= 2) {
        auto cgal_delete = [&](bool afterInsert = 1, double ratio = 1.0) {
            if (!afterInsert) {
                tree.clear();
                tree.insert(_points.begin(), _points.end());
            }
            timer.reset();
            timer.start();
            if (afterInsert) {
                size_t sz = _points_insert.size() * ratio;
                tree.remove(_points_insert.begin(), _points_insert.begin() + sz);
            } else {
                assert(tree.size() == wp.size());
                size_t sz = _points.size() * ratio;
                tree.remove(_points.begin(), _points.begin() + sz);
            }
            timer.stop();
            std::cout << timer.total_time() << " " << std::flush;
        };

        if (summary) {
            const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
            for (int i = 0; i < ratios.size(); i++) {
                cgal_delete(0, ratios[i]);
            }
            tree.clear();
            tree.insert(_points.begin(), _points.end());
        } else {
            cgal_delete(0, batchInsertRatio);
        }
    }

    // PERF: handle the size of cgknn dynamically
    Typename* cgknn;
    if (tag == 1) {
        cgknn = new Typename[N + wi.size()];
    } else {
        cgknn = new Typename[N];
    }

    // kNN query: find the 3 nearest neighbors to the point (2.5, 2.5)
    // Point query_point(2.5, 2.5);
    // std::vector<Point> knn_results;
    // rtree.query(bgi::nearest(query_point, 3), std::back_inserter(knn_results));
    //
    // std::cout << "kNN query results (3 nearest to (2.5, 2.5)):" << std::endl;
    // for (const auto& v : knn_results) {
    //     std::cout << "Point: (" << v.first.get<0>() << ", " << v.first.get<1>() << "), ID: " << v.second <<
    //     std::endl;
    // }
    if (queryType & (1 << 0)) {  // NOTE: KNN query
        auto run_rtree_knn = [&](int kth, size_t batchSize) {
            timer.reset();
            timer.start();
            parlay::sequence<size_t> visNodeNum(batchSize, 0);
            parlay::parallel_for(0, batchSize, [&](size_t i) {
                RPoint query_point(wp[i].pnt[0], wp[i].pnt[1]);
                std::vector<RPoint> knn_results;
                tree.query(bgi::nearest(query_point, kth), std::back_inserter(knn_results));
            });
            timer.stop();
            std::cout << timer.total_time() << " " << -1 << " " << -1 << " " << std::flush;
        };

        size_t batchSize = static_cast<size_t>(wp.size() * batchQueryRatio);
        if (summary == 0) {
            const int k[3] = {1, 10, 100};
            for (int i = 0; i < 3; i++) {
                run_rtree_knn(k[i], batchSize);
            }
        } else {
            run_rtree_knn(K, batchSize);
        }
    }

    //
    // // Range query: find points within the box defined by (1.5, 1.5) and (4.5, 4.5)
    // Box query_box(Point(1.5, 1.5), Point(4.5, 4.5));
    // std::vector<Point> range_results;
    // rtree.query(bgi::intersects(query_box), std::back_inserter(range_results));
    //
    if (queryType & (1 << 3)) {  // NOTE: range query
        auto run_cgal_range_query = [&](int type) {
            size_t n = wp.size();
            int queryNum = summary ? summaryRangeQueryNum : rangeQueryNum;
            auto [queryBox, maxSize] = gen_rectangles(queryNum, type, wp, Dim);
            // using ref_t = std::reference_wrapper<Point_d>;
            // std::vector<ref_t> out_ref( queryNum * maxSize, std::ref( _points[0] )
            // );
            std::vector<RPoint> _ans(queryNum * maxSize);

            timer.reset();
            timer.start();
            if (summary) {
                parlay::parallel_for(0, queryNum, [&](size_t s) {
                    RBox query_box(RPoint(queryBox[s].first.first.pnt[0], queryBox[s].first.first.pnt[1]),
                                   RPoint(queryBox[s].first.second.pnt[0], queryBox[s].first.second.pnt[1]));
                    std::vector<RPoint> range_results;
                    // tree.query(bgi::intersects(query_box), std::back_inserter(range_results));
                    tree.query(bgi::within(query_box), std::back_inserter(range_results));
                });
                timer.stop();
                std::cout << timer.total_time() << " " << std::flush;
            } else {
                for (int s = 0; s < queryNum; s++) {
                    auto aveQuery = time_loop(
                        singleQueryLogRepeatNum, -1.0, [&]() {},
                        [&]() {
                            RBox query_box(RPoint(queryBox[s].first.first.pnt[0], queryBox[s].first.first.pnt[1]),
                                           RPoint(queryBox[s].first.second.pnt[0], queryBox[s].first.second.pnt[1]));
                            std::vector<RPoint> range_results;
                            // tree.query(bgi::intersects(query_box), std::back_inserter(range_results));
                            tree.query(bgi::within(query_box), std::back_inserter(range_results));
                        },
                        [&]() {});
                    // LOG << queryBox[s].second << " " << std::scientific << aveQuery << ENDL;
                }
            }
        };

        if (summary == 0) {
            LOG << ENDL;
            const int type[3] = {0, 1, 2};
            for (int i = 0; i < 3; i++) {
                run_cgal_range_query(type[i]);
            }
        } else {
            run_cgal_range_query(2);
        }
    }
    //
    // // Range query: find points within the box defined by (1.5, 1.5) and (4.5, 4.5)
    // Box query_box(Point(1.5, 1.5), Point(4.5, 4.5));
    // std::vector<Point> range_results;
    // rtree.query(bgi::intersects(query_box), std::back_inserter(range_results));
    //
    // std::cout << "\nRange query results (points within box (1.5, 1.5) to (4.5, 4.5)):" << std::endl;
    // for (const auto& v : range_results) {
    //     std::cout << "Point: (" << v.first.get<0>() << ", " << v.first.get<1>() << "), ID: " << v.second <<
    //     std::endl;
    // }
    //
    // // Batch deletion: delete points within the box (1.5, 1.5) to (4.5, 4.5)
    // for (const auto& v : range_results) {
    //     rtree.remove(v);
    // }
    //
    // // Confirm deletion with another range query
    // std::vector<Point> post_delete_results;
    // rtree.query(bgi::intersects(query_box), std::back_inserter(post_delete_results));
    //
    // std::cout << "\nRange query results after deletion:" << std::endl;
    // if (post_delete_results.empty()) {
    //     std::cout << "No points found within the box (1.5, 1.5) to (4.5, 4.5)" << std::endl;
    // } else {
    //     for (const auto& v : post_delete_results) {
    //         std::cout << "Point: (" << v.first.get<0>() << ", " << v.first.get<1>() << "), ID: " << v.second
    //                   << std::endl;
    //     }
    // }
}

int main(int argc, char* argv[]) {
    commandLine P(argc, argv,
                  "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                  "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i "
                  "<_insertFile>] [-s <summary>]");

    char* iFile = P.getOptionValue("-p");
    int K = P.getOptionIntValue("-k", 100);
    int Dim = P.getOptionIntValue("-d", 3);
    size_t N = P.getOptionLongValue("-n", -1);
    int tag = P.getOptionIntValue("-t", 1);
    int rounds = P.getOptionIntValue("-r", 3);
    int queryType = P.getOptionIntValue("-q", 0);
    int readInsertFile = P.getOptionIntValue("-i", 1);
    int summary = P.getOptionIntValue("-s", 0);

    using point = PointType<coord, 10>;
    using points = parlay::sequence<point>;

    points wp;
    std::string name, insertFile = "";
    int LEAVE_WRAP = 32;

    //* initialize points
    if (iFile != NULL) {
        name = std::string(iFile);
        name = name.substr(name.rfind("/") + 1);
        std::cout << name << " ";
        auto [n, d] = read_points<point>(iFile, wp, K);
        N = n;
        assert(Dim == d);
    } else {  //* construct data byself
        K = 100;
        generate_random_points<point>(wp, 10000, N, Dim);
        assert(wp.size() == N);
        std::string name = std::to_string(N) + "_" + std::to_string(Dim) + ".in";
        std::cout << name << " ";
    }

    if (readInsertFile == 1) {
        int id = std::stoi(name.substr(0, name.find_first_of('.')));
        id = (id + 1) % 3;  //! MOD graph number used to test
        if (!id) id++;
        int pos = std::string(iFile).rfind("/") + 1;
        insertFile = std::string(iFile).substr(0, pos) + std::to_string(id) + ".in";
    }

    assert(N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1);

    if (tag == -1) {
        //* serial run
        // todo rewrite test serial code
        // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    } else if (Dim == 2) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 2> { return PointType<coord, 2>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testRtreeParallel<PointType<coord, 2>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 3) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 3> { return PointType<coord, 3>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testRtreeParallel<PointType<coord, 3>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 5) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 5> { return PointType<coord, 5>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testRtreeParallel<PointType<coord, 5>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 7) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 7> { return PointType<coord, 7>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testRtreeParallel<PointType<coord, 7>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 9) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 9> { return PointType<coord, 9>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testRtreeParallel<PointType<coord, 9>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 10) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 10> { return PointType<coord, 10>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testRtreeParallel<PointType<coord, 10>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                summary);
    }
    return 0;
}
