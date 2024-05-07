#include "common/parse_command_line.h"

#include "parlay/internal/get_time.h"
#include "parlay/parallel.h"
#include "testFramework.h"

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Fuzzy_sphere.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <cstddef>
#include <tbb/task_scheduler_init.h>
#include <tbb/task_arena.h>
#define TBB_PREVIEW_GLOBAL_CONTROL 1
#include <tbb/global_control.h>

using Typename = coord;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;
typedef CGAL::Fuzzy_sphere<TreeTraits> Fuzzy_circle;
//@ median tree
typedef CGAL::Median_of_rectangle<TreeTraits> Median_of_rectangle;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Median_of_rectangle> Neighbor_search_Median;
typedef Neighbor_search_Median::Tree Tree_Median;

//@ midpoint tree
typedef CGAL::Midpoint_of_rectangle<TreeTraits> Midpoint_of_rectangle;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance, Midpoint_of_rectangle> Neighbor_search_Midpoint;
typedef Neighbor_search_Midpoint::Tree Tree_Midpoint;
typedef CGAL::Fuzzy_iso_box<TreeTraits> Fuzzy_iso_box;

template<typename Splitter, typename Tree, typename Neighbor_search, typename point>
void testCGALParallel(int Dim, int LEAVE_WRAP, parlay::sequence<point>& wp, int N, int K, const int& rounds,
                      const string& insertFile, const int& tag, const int& queryType, const int summary) {
    using points = parlay::sequence<point>;
    using pkdTree = ParallelKDtree<point>;
    using box = typename pkdTree::box;

    // NOTE: set cgal threads number
    // TODO: remove it before test summary
    // int nthreads = std::stoi(std::getenv("TEST_CGAL_THREADS"));
    // tbb::task_scheduler_init TBBinit(nthreads); // Decrapted
    // NOTE: Limit the number of threads to two for all oneTBB parallel interfaces
    // tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, nthreads);

    parlay::internal::timer timer;

    points wi;
    if (insertFile != "") {
        auto [nn, nd] = read_points<point>(insertFile.c_str(), wi, K);
        if (nd != Dim) {
            puts("read inserted points dimension wrong");
            abort();
        }
    }
    //* otherwise cannot insert heavy duplicated points
    // wp = parlay::unique( parlay::sort( wp ),
    //                      [&]( const point& a, const point& b ) { return a == b;
    //                      } );
    // wi = parlay::unique( parlay::sort( wi ),
    //                      [&]( const point& a, const point& b ) { return a == b;
    //                      } );
    N = wp.size();

    //* cgal
    std::vector<Point_d> _points(N);
    std::vector<Point_d> _points_insert(wi.size());
    parlay::parallel_for(
        0, N, [&](size_t i) { _points[i] = Point_d(Dim, std::begin(wp[i].pnt), (std::begin(wp[i].pnt) + Dim)); });
    parlay::parallel_for(0, wi.size(), [&](size_t i) {
        _points_insert[i] = Point_d(Dim, std::begin(wi[i].pnt), (std::begin(wi[i].pnt) + Dim));
    });

    timer.start();
    Splitter split;
    Tree tree(_points.begin(), _points.end(), split);
    tree.template build<CGAL::Parallel_tag>();
    timer.stop();

    std::cout << timer.total_time() << " " << tree.root()->depth() << " " << std::flush;

    if (tag >= 1) {
        auto cgal_insert = [&](double r) {
            timer.reset();
            timer.start();
            size_t sz = _points_insert.size() * r;
            tree.insert(_points_insert.begin(), _points_insert.begin() + sz);
            tree.template build<CGAL::Parallel_tag>();
            std::cout << timer.total_time() << " " << std::flush;
        };

        if (summary) {
            const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
            for (int i = 0; i < ratios.size(); i++) {
                tree.clear();
                tree.insert(_points.begin(), _points.end());
                tree.template build<CGAL::Parallel_tag>();
                cgal_insert(ratios[i]);
            }
        } else {
            cgal_insert(batchInsertRatio);
        }

        if (tag == 1) wp.append(wi);
    }

    if (tag >= 2) {
        auto cgal_delete = [&](bool afterInsert = 1, double ratio = 1.0) {
            if (!afterInsert) {
                tree.clear();
                tree.insert(_points.begin(), _points.end());
                tree.template build<CGAL::Parallel_tag>();
            }
            timer.reset();
            timer.start();
            if (afterInsert) {
                size_t sz = _points_insert.size() * ratio;
                for (auto it = _points_insert.begin(); it != _points_insert.begin() + sz; it++) {
                    tree.remove(*it);
                }
            } else {
                assert(tree.size() == wp.size());
                size_t sz = _points.size() * ratio;
                for (auto it = _points.begin(); it != _points.begin() + sz; it++) {
                    tree.remove(*it);
                }
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
            tree.template build<CGAL::Parallel_tag>();
        } else {
            cgal_delete(0, batchInsertRatio);
        }
    }

    //* start test

    // PERF: handle the size of cgknn dynamically
    Typename* cgknn;
    if (tag == 1) {
        cgknn = new Typename[N + wi.size()];
    } else {
        cgknn = new Typename[N];
    }

    if (queryType & (1 << 0)) {  // NOTE: KNN query
        auto run_cgal_knn = [&](int kth, size_t batchSize) {
            timer.reset();
            timer.start();
            parlay::sequence<size_t> visNodeNum(batchSize, 0);
            tbb::parallel_for(tbb::blocked_range<std::size_t>(0, batchSize),
                              [&](const tbb::blocked_range<std::size_t>& r) {
                                  for (std::size_t s = r.begin(); s != r.end(); ++s) {
                                      // Neighbor search can be instantiated from
                                      // several threads at the same time
                                      Point_d query(Dim, std::begin(wp[s].pnt), std::begin(wp[s].pnt) + Dim);
                                      Neighbor_search search(tree, query, kth);
                                      auto it = search.end();
                                      it--;
                                      cgknn[s] = it->second;
                                      visNodeNum[s] = search.internals_visited() + search.leafs_visited();
                                  }
                              });
            timer.stop();
            std::cout << timer.total_time() << " " << tree.root()->depth() << " "
                      << parlay::reduce(visNodeNum) / batchSize << " " << std::flush;
        };

        size_t batchSize = static_cast<size_t>(wp.size() * batchQueryRatio);
        if (summary == 0) {
            const int k[3] = {1, 10, 100};
            for (int i = 0; i < 3; i++) {
                run_cgal_knn(k[i], batchSize);
            }
        } else {
            run_cgal_knn(K, batchSize);
        }
    }

    if (queryType & (1 << 1)) {  // NOTE: batch query
        auto cgal_batch_knn = [&](points& pts, size_t batchSize) {
            timer.reset();
            timer.start();
            parlay::sequence<size_t> visNodeNum(batchSize, 0);
            tbb::parallel_for(tbb::blocked_range<std::size_t>(0, batchSize),
                              [&](const tbb::blocked_range<std::size_t>& r) {
                                  for (std::size_t s = r.begin(); s != r.end(); ++s) {
                                      // Neighbor search can be instantiated from
                                      // several threads at the same time
                                      Point_d query(Dim, std::begin(pts[s].pnt), std::begin(pts[s].pnt) + Dim);
                                      Neighbor_search search(tree, query, K);
                                      auto it = search.end();
                                      it--;
                                      cgknn[s] = it->second;
                                      visNodeNum[s] = search.internals_visited() + search.leafs_visited();
                                  }
                              });
            timer.stop();
            std::cout << timer.total_time() << " " << tree.root()->depth() << " "
                      << parlay::reduce(visNodeNum) / wp.size() << " " << std::flush;
        };

        cgal_batch_knn(wp, static_cast<size_t>(wp.size() * batchQueryRatio));
        // const std::vector<double> batchRatios = {0.001, 0.01, 0.1, 0.2, 0.5};
        // for (auto r : batchRatios) {
        //   cgal_batch_knn(wp, static_cast<size_t>(wp.size() * r));
        // }
        // for (auto r : batchRatios) {
        //   cgal_batch_knn(wi, static_cast<size_t>(wp.size() * r));
        // }
    }

    if (queryType & (1 << 2)) {  // NOTE: range count
        int queryNum = rangeQueryNum;
        const int type[3] = {0, 1, 2};
        for (int i = 0; i < 3; i++) {
            size_t n = wp.size();
            std::vector<Point_d> _ans(n);
            auto [queryBox, maxSize] = gen_rectangles(queryNum, type[i], wp, Dim);

            timer.reset();
            timer.start();

            parlay::parallel_for(0, queryNum, [&](size_t i) {
                Point_d a(Dim, std::begin(queryBox[i].first.first.pnt), std::end(queryBox[i].first.first.pnt)),
                    b(Dim, std::begin(queryBox[i].first.second.pnt), std::end(queryBox[i].first.second.pnt));
                Fuzzy_iso_box fib(a, b, 0.0);

                size_t cnt = 0;
                counter_iterator<size_t> cnt_iter(cnt);

                auto it = tree.search(cnt_iter, fib);
                cgknn[i] = cnt;
            });

            timer.stop();
            std::cout << timer.total_time() << " " << std::flush;
        }
    }

    if (queryType & (1 << 3)) {  // NOTE: range query
        auto run_cgal_range_query = [&](int type) {
            size_t n = wp.size();
            int queryNum = summary ? summaryRangeQueryNum : rangeQueryNum;
            auto [queryBox, maxSize] = gen_rectangles(queryNum, type, wp, Dim);
            // using ref_t = std::reference_wrapper<Point_d>;
            // std::vector<ref_t> out_ref( queryNum * maxSize, std::ref( _points[0] )
            // );
            std::vector<Point_d> _ans(queryNum * maxSize);

            timer.reset();
            timer.start();
            if (summary) {
                tbb::parallel_for(tbb::blocked_range<std::size_t>(0, queryNum),
                                  [&](const tbb::blocked_range<std::size_t>& r) {
                                      for (std::size_t s = r.begin(); s != r.end(); ++s) {
                                          Point_d a(Dim, std::begin(queryBox[s].first.first.pnt),
                                                    std::end(queryBox[s].first.first.pnt)),
                                              b(Dim, std::begin(queryBox[s].first.second.pnt),
                                                std::end(queryBox[s].first.second.pnt));
                                          Fuzzy_iso_box fib(a, b, 0.0);
                                          auto it = tree.search(_ans.begin() + s * maxSize, fib);
                                          cgknn[s] = std::distance(_ans.begin() + s * maxSize, it);
                                      }
                                  });
                timer.stop();
                std::cout << timer.total_time() << " " << std::flush;
            } else {
                for (int s = 0; s < queryNum; s++) {
                    timer.reset(), timer.start();

                    Point_d a(Dim, std::begin(queryBox[s].first.first.pnt), std::end(queryBox[s].first.first.pnt)),
                        b(Dim, std::begin(queryBox[s].first.second.pnt), std::end(queryBox[s].first.second.pnt));
                    Fuzzy_iso_box fib(a, b, 0.0);
                    auto it = tree.search(_ans.begin() + s * maxSize, fib);
                    cgknn[s] = std::distance(_ans.begin() + s * maxSize, it);

                    timer.stop();
                    LOG << queryBox[s].second << " " << timer.total_time() << ENDL;
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

    if (queryType & (1 << 4)) {  //* batch insert with fraction
        const parlay::sequence<double> ratios = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                                                 0.02,   0.05,   0.1,    0.2,   0.5,   1.0};
        for (int i = 0; i < ratios.size(); i++) {
            tree.clear();

            //* build tree
            tree.insert(_points.begin(), _points.end());
            tree.template build<CGAL::Parallel_tag>();

            auto sz = size_t(wi.size() * ratios[i]);

            timer.reset(), timer.start();
            tree.insert(_points_insert.begin(), _points_insert.begin() + sz);
            tree.template build<CGAL::Parallel_tag>();
            timer.stop();

            std::cout << timer.total_time() << " " << std::flush;
        }
    }

    if (queryType & (1 << 5)) {  //* batch deletion with fraction
        const parlay::sequence<double> ratios = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                                                 0.02,   0.05,   0.1,    0.2,   0.5,   1.0};
        for (int i = 0; i < ratios.size(); i++) {
            tree.clear();

            //* build tree
            // _points.resize(wp.size());
            // N = wp.size();
            tree.insert(_points.begin(), _points.end());
            tree.template build<CGAL::Parallel_tag>();

            auto sz = size_t(wp.size() * ratios[i]);
            timer.reset(), timer.start();
            for (size_t j = 0; j < sz; j++) {
                tree.remove(_points[j]);
            }
            timer.stop();
            std::cout << timer.total_time() << " " << std::flush;
        }
    }

    if (queryType & (1 << 6)) {  //* incremental construct
        double ratios[4] = {0.1, 0.2, 0.25, 0.5};
        for (int i = 0; i < 4; i++) {
            std::cout << "-1 -1 " << std::flush;
        }
    }

    if (queryType & (1 << 7)) {  //* incremental delete
        double ratios[4] = {0.1, 0.2, 0.25, 0.5};
        for (int i = 0; i < 4; i++) {
            std::cout << "-1 -1 " << std::flush;
        }
    }

    if (queryType & (1 << 8)) {  //* incremental then knn
        std::cout << "-1 -1 -1 " << std::flush;
        std::cout << "-1 -1 -1 " << std::flush;
    }

    if (queryType & (1 << 9)) {  //* decremental then knn
        std::cout << "-1 -1 -1 " << std::flush;
        std::cout << "-1 -1 -1 " << std::flush;
    }

    auto queryPointCgal = [&](const int kth, const std::vector<Point_d>& all_pts) {
        timer.reset();
        timer.start();
        parlay::sequence<size_t> visNodeNum(all_pts.size(), 0);
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, all_pts.size()),
                          [&](const tbb::blocked_range<std::size_t>& r) {
                              for (std::size_t s = r.begin(); s != r.end(); ++s) {
                                  // Neighbor search can be instantiated from
                                  // several threads at the same time
                                  Point_d query = all_pts[s];
                                  Neighbor_search search(tree, query, kth);
                                  auto it = search.end();
                                  it--;
                                  cgknn[s] = it->second;
                                  visNodeNum[s] = search.internals_visited() + search.leafs_visited();
                              }
                          });
        timer.stop();
        std::cout << timer.total_time() << " " << tree.root()->depth() << " "
                  << parlay::reduce(visNodeNum) / all_pts.size() << " " << std::flush;
    };

    auto insertOsmByTimaCgal = [&](const std::vector<std::vector<Point_d>>& pts) {
        int fileNum = pts.size();
        tree.clear();
        parlay::internal::timer timer;
        for (int i = 0; i < fileNum; i++) {
            timer.reset(), timer.start();
            tree.insert(pts[i].begin(), pts[i].end());
            tree.template build<CGAL::Parallel_tag>();
            timer.stop();
            LOG << pts[i].size() << " " << timer.total_time() << " " << ENDL;

            if (fileNum < 12) {
                std::vector<Point_d> tmp(pts[0].begin(), pts[0].begin() + batchQueryOsmSize);
                queryPointCgal(K, tmp);
            } else if (i != 0 && (i + 1) % 12 == 0) {
                std::vector<Point_d> tmp( batchQueryOsmSize);
                parlay::copy(parlay::make_slice(pts[0]), tmp.cut(0, pts[0].size()));
                parlay::copy(parlay::make_slice(pts[1].begin(), pts[1].begin()+batchQueryOsmSize-pts[0].size()), tmp.cut(pts[0].size(), tmp.size()));
                queryPointCgal(K);
            }
        }
    };

    if (queryType & (1 << 11)) {  // NOTE: osm by year
        LOG << ENDL;

        // WARN: remember using double
        string osm_prefix = "/data/zmen002/kdtree/real_world/osm/year/";
        const std::vector<std::string> files = {"2014", "2015", "2016", "2017", "2018",
                                                "2019", "2020", "2021", "2022", "2023"};
        parlay::sequence<points> node_by_year(files.size());
        for (int i = 0; i < files.size(); i++) {
            std::string path = osm_prefix + "osm_" + files[i] + ".csv";
            read_points(path.c_str(), node_by_year[i], K);
        }

        std::vector<std::vector<Point_d>> pts(files.size());
        for (int i = 0; i < files.size(); i++) {
            pts[i].resize(node_by_year[i].size());
            parlay::parallel_for(0, node_by_year[i].size(), [&](size_t j) {
                pts[i][j] = Point_d(Dim, std::begin(node_by_year[i][j].pnt), std::end(node_by_year[i][j].pnt));
            });
        }
        insertOsmByTimaCgal(pts);

        auto all_points = parlay::flatten(node_by_year);
        cgknn = new Typename[all_points.size()];
        std::vector<Point_d> all_pts(all_points.size());
        parlay::parallel_for(0, all_points.size(), [&](size_t j) {
            all_pts[j] = Point_d(Dim, std::begin(all_points[j].pnt), std::end(all_points[j].pnt));
        });
        queryPointCgal(K, all_pts);
        delete[] cgknn;
    }

    if (queryType & (1 << 12)) {  // NOTE: osm by month
        LOG << ENDL;
        // WARN: remember using double
        string osm_prefix = "/data/zmen002/kdtree/real_world/osm/month/";
        const std::vector<std::string> files = {"2014", "2015", "2016", "2017", "2018",
                                                "2019", "2020", "2021", "2022", "2023"};
        const std::vector<std::string> month = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};

        parlay::sequence<points> node(files.size() * month.size());
        for (int i = 0; i < files.size(); i++) {
            for (int j = 0; j < month.size(); j++) {
                std::string path = osm_prefix + files[i] + "/" + month[j] + ".csv";
                read_points(path.c_str(), node[i * month.size() + j], K);
            }
        }

        std::vector<std::vector<Point_d>> pts(node.size());
        for (int i = 0; i < files.size(); i++) {
            for (int j = 0; j < month.size(); j++) {
                int idx = i * month.size() + j;
                pts[idx].resize(node[idx].size());
                parlay::parallel_for(0, node[idx].size(), [&](size_t k) {
                    pts[idx][k] = Point_d(Dim, std::begin(node[idx][k].pnt), std::end(node[idx][k].pnt));
                });
            }
        }
        insertOsmByTimaCgal(pts);

        auto all_points = parlay::flatten(node);
        std::vector<Point_d> all_pts(all_points.size());
        cgknn = new Typename[all_points.size()];
        parlay::parallel_for(0, all_points.size(), [&](size_t j) {
            all_pts[j] = Point_d(Dim, std::begin(all_points[j].pnt), std::end(all_points[j].pnt));
        });
        queryPointCgal(K, all_pts);
        delete[] cgknn;
    }
    std::cout << std::endl << std::flush;

    return;
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
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 2>>(
            Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 3) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 3> { return PointType<coord, 3>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 3>>(
            Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 5) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 5> { return PointType<coord, 5>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 5>>(
            Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 7) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 7> { return PointType<coord, 7>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 7>>(
            Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 9) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 9> { return PointType<coord, 9>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 9>>(
            Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    } else if (Dim == 10) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 10> { return PointType<coord, 10>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 10>>(
            Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType, summary);
    }

    // else if ( tag == -1 )
    //   testCGALSerial<Median_of_rectangle, Tree_Median, Neighbor_search_Median>(
    //       Dim, LEAVE_WRAP, wp, N, K );

    return 0;
}
