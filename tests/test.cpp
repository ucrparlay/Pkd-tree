#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include "parlay/primitives.h"
#include "testFramework.h"

template<typename point>
void testParallelKDtree(const int& Dim, const int& LEAVE_WRAP, parlay::sequence<point>& wp, const size_t& N,
                        const int& K, const int& rounds, const string& insertFile, const int& tag, const int& queryType,
                        const int readInsertFile, const int summary) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using interior = typename tree::interior;
    using leaf = typename tree::leaf;
    using node_tag = typename tree::node_tag;
    using node_tags = typename tree::node_tags;
    using box = typename tree::box;
    using boxs = parlay::sequence<box>;

    // auto boxs = gen_rectangles<point>( 1000, 2, wp, Dim );
    // for ( int i = 0; i < 10; i++ ) {
    //   LOG << boxs[i].first << " " << boxs[i].second << ENDL;
    // }
    // return;

    if (N != wp.size()) {
        puts("input parameter N is different to input points size");
        abort();
    }

    tree pkd;

    points wi;
    if (readInsertFile && insertFile != "") {
        auto [nn, nd] = read_points<point>(insertFile.c_str(), wi, K);
        if (nd != Dim) {
            puts("read inserted points dimension wrong");
            abort();
        }
    }

    Typename* kdknn = nullptr;

    //* begin test
    buildTree<point>(Dim, wp, rounds, pkd);

    //* batch insert
    if (tag >= 1) {
        if (summary) {
            const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
            for (int i = 0; i < ratios.size(); i++) {
                batchInsert<point>(pkd, wp, wi, Dim, rounds, ratios[i]);
            }
        } else {
            batchInsert<point>(pkd, wp, wi, Dim, rounds, batchInsertRatio);
        }
    }

    //* batch delete
    if (tag >= 2) {
        if (summary) {
            const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
            for (int i = 0; i < ratios.size(); i++) {
                batchDelete<point>(pkd, wp, wi, Dim, rounds, 0, ratios[i]);
            }
        } else {
            batchDelete<point>(pkd, wp, wi, Dim, rounds, 0, batchInsertRatio);
        }
    }

    if (queryType & (1 << 0)) {  // NOTE: KNN
        auto run_batch_knn = [&](const points& pts, int kth, size_t batchSize) {
            points newPts(batchSize);
            parlay::copy(pts.cut(0, batchSize), newPts.cut(0, batchSize));
            kdknn = new Typename[batchSize];
            queryKNN<point>(Dim, newPts, rounds, pkd, kdknn, kth, true);
            delete[] kdknn;
        };

        size_t batchSize = static_cast<size_t>(wp.size() * batchQueryRatio);

        if (summary == 0) {
            int k[3] = {1, 10, 100};
            for (int i = 0; i < 3; i++) {
                run_batch_knn(wp, k[i], batchSize);
            }
        } else {  // test summary
            run_batch_knn(wp, K, batchSize);
        }
    }

    if (queryType & (1 << 1)) {  // NOTE: batch NN query

        // auto run_batch_knn = [&](const points& pts, size_t
        // batchSize) {
        //   points newPts(batchSize);
        //   parlay::copy(pts.cut(0, batchSize), newPts.cut(0,
        //   batchSize)); kdknn = new Typename[batchSize];
        //   queryKNN<point, true, true>(Dim, newPts, rounds, pkd,
        //   kdknn, K, true); delete[] kdknn;
        // };
        //
        // run_batch_knn(wp, static_cast<size_t>(wp.size() *
        // batchQueryRatio));
        // const std::vector<double> batchRatios = {0.001, 0.01, 0.1, 0.2, 0.5};
        // for (auto ratio : batchRatios) {
        //   run_batch_knn(wp, static_cast<size_t>(wp.size() * ratio));
        // }
        // for (auto ratio : batchRatios) {
        //   run_batch_knn(wi, static_cast<size_t>(wi.size() * ratio));
        // }
    }

    if (queryType & (1 << 2)) {  // NOTE: range count
        int recNum = rangeQueryNum;
        kdknn = new Typename[recNum];
        const int type[3] = {0, 1, 2};

        LOG << ENDL;
        for (int i = 0; i < 3; i++) {
            rangeCountFixWithLog<point>(wp, pkd, kdknn, singleQueryLogRepeatNum, type[i], recNum, Dim);
        }
        // rangeCountFix<point>(wp, pkd, kdknn, rounds, 2, rangeQueryNumInbaRatio, Dim);
        // rangeCountFix<point>(wp, pkd, kdknn, rounds, 2, recNum, Dim);

        delete[] kdknn;
    }

    if (queryType & (1 << 3)) {  // NOTE: range query
        if (summary == 0) {
            int recNum = rangeQueryNum;
            const int type[3] = {0, 1, 2};

            LOG << ENDL;
            for (int i = 0; i < 3; i++) {
                //* run range count to obtain size
                kdknn = new Typename[recNum];
                points Out;
                rangeQuerySerialWithLog<point>(wp, pkd, kdknn, singleQueryLogRepeatNum, Out, type[i], recNum, Dim);
                delete[] kdknn;
            }
        } else if (summary == 1) {  // NOTE: for summary
            kdknn = new Typename[summaryRangeQueryNum];
            points Out;
            rangeQueryFix<point>(wp, pkd, kdknn, rounds, Out, 2, summaryRangeQueryNum, Dim);
            delete[] kdknn;
        }
    }

    if (queryType & (1 << 4)) {  // NOTE: batch insertion with fraction
        const parlay::sequence<double> ratios = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                                                 0.02,   0.05,   0.1,    0.2,   0.5,   1.0};
        for (int i = 0; i < ratios.size(); i++) {
            batchInsert<point>(pkd, wp, wi, Dim, rounds, ratios[i]);
        }
    }

    if (queryType & (1 << 5)) {  // NOTE: batch deletion with fraction
        const parlay::sequence<double> ratios = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                                                 0.02,   0.05,   0.1,    0.2,   0.5,   1.0};
        points tmp;
        for (int i = 0; i < ratios.size(); i++) {
            batchDelete<point>(pkd, wp, tmp, Dim, rounds, 0, ratios[i]);
        }
    }

    if (queryType & (1 << 6)) {  //* incremental Build
        double step[4] = {0.1, 0.2, 0.25, 0.5};
        for (int i = 0; i < 4; i++) {
            incrementalBuild<point>(Dim, wp, rounds, pkd, step[i]);
        }
    }

    if (queryType & (1 << 7)) {  //* incremental Delete
        double step[4] = {0.1, 0.2, 0.25, 0.5};
        for (int i = 0; i < 4; i++) {
            incrementalDelete<point>(Dim, wp, wi, rounds, pkd, step[i]);
        }
    }

    if (queryType & (1 << 8)) {  //* batch insertion then knn
        kdknn = new Typename[wp.size()];

        //* first normal build
        buildTree<point, 0>(Dim, wp, rounds, pkd);
        queryKNN<point>(Dim, wp, rounds, pkd, kdknn, K, false);

        //* then incremental build
        incrementalBuild<point, 0>(Dim, wp, rounds, pkd, 0.1);
        queryKNN<point>(Dim, wp, rounds, pkd, kdknn, K, false);

        delete[] kdknn;
    }

    if (queryType & (1 << 9)) {  //* batch deletion then knn
        kdknn = new Typename[wp.size()];

        //* first normal build
        buildTree<point, 0>(Dim, wp, rounds, pkd);
        queryKNN<point>(Dim, wp, rounds, pkd, kdknn, K, false);

        //* then incremental delete
        incrementalDelete<point, 0>(Dim, wp, wi, rounds, pkd, 0.1);
        queryKNN<point>(Dim, wp, rounds, pkd, kdknn, K, false);

        delete[] kdknn;
    }

    if (queryType & (1 << 10)) {  // NOTE: test inbalance ratio
        const int fileNum = 10;

        const size_t batchPointNum = wp.size() / fileNum;

        points np, nq, up;
        std::string prefix, path;
        const string insertFileBack = insertFile;
        const string ten_varden_path = "/data/zmen002/kdtree/ss_varden/1000000000_3/10V.in";
        const string one_uniform_nine_varden = "/data/zmen002/kdtree/ss_varden/1000000000_3/1U9V.in";
        const string uniform_path = "/data/zmen002/kdtree/uniform/1000000000_3/1.in";

        auto inbaQueryType = std::stoi(std::getenv("INBA_QUERY"));
        auto inbaBuildType = std::stoi(std::getenv("INBA_BUILD"));

        // NOTE: helper functions
        auto clean = [&]() {
            prefix = insertFile.substr(0, insertFile.rfind("/"));
            np.clear();
            nq.clear();
        };

        auto writeToFile = [&](string path) {
            std::ofstream f(path);
            f << np.size() << " " << Dim << std::endl;
            for (size_t i = 0; i < np.size(); i++) {
                for (size_t j = 0; j < Dim; j++) {
                    f << np[i].pnt[j] << " ";
                }
                f << std::endl;
            }
            f.close();
        };

        // NOTE: run the test
        auto run = [&]() {
            if (inbaBuildType == 0) {
                buildTree<point, 2>(Dim, np, rounds, pkd);
            } else {
                // incrementalBuild<point, 2>(Dim, np, rounds, pkd, insertBatchInbaRatio);

                size_t batchSize = static_cast<size_t>(up.size() * knnBatchInbaRatio);
                points newPts(batchSize);
                parlay::copy(up.cut(0, batchSize), newPts.cut(0, batchSize));
                incrementalBuildAndQuery<point, 2>(Dim, np, rounds, pkd, insertBatchInbaRatio, newPts);
            }

            // if (inbaQueryType == 0) {
            //     size_t batchSize = static_cast<size_t>(np.size() * knnBatchInbaRatio);
            //     points newPts(batchSize);
            //     parlay::copy(np.cut(0, batchSize), newPts.cut(0, batchSize));
            //     kdknn = new Typename[batchSize];
            //     const int k[3] = {1, 5, 100};
            //     for (int i = 0; i < 3; i++) {
            //         queryKNN<point, 0, 1>(Dim, newPts, rounds, pkd, kdknn, k[i], true);
            //     }
            //     delete[] kdknn;
            // } else if (inbaQueryType == 1) {
            //     kdknn = new Typename[rangeQueryNumInbaRatio];
            //     int type = 2;
            //     rangeCountFix<point>(np, pkd, kdknn, rounds, type, rangeQueryNumInbaRatio, Dim);
            //     delete[] kdknn;
            // }
        };

        read_points(uniform_path.c_str(), up, K);

        LOG << "alpha: " << pkd.get_imbalance_ratio() << ENDL;
        // HACK: need start with varden file
        // NOTE: 1: 10*0.1 different vardens.
        clean();
        // for (int i = 1; i <= fileNum; i++) {
        //     path = prefix + "/" + std::to_string(i) + ".in";
        //     // std::cout << path << std::endl;
        //     read_points<point>(path.c_str(), nq, K);
        //     np.append(nq.cut(0, batchPointNum));
        //     nq.clear();
        // }
        // writeToFile(ten_varden_path);
        read_points(ten_varden_path.c_str(), np, K);
        assert(np.size() == wp.size());
        run();

        // NOTE: 2: 1 uniform, and 9*0.1 same varden
        //* read varden first
        clean();
        // path = prefix + "/1.in";
        // std::cout << "varden path" << path << std::endl;
        // read_points<point>(path.c_str(), np, K);
        //* then read uniforprefixm
        // prefix = prefix.substr(0, prefix.rfind("/"));  // 1000000_3
        // prefix = prefix.substr(0, prefix.rfind("/"));  // ss_varden
        // path = prefix + "/uniform/" + std::to_string(wp.size()) + "_" + std::to_string(Dim) + "/1.in";
        // std::cout << "uniform path:" << path << std::endl;

        // read_points<point>(path.c_str(), nq, K);
        // parlay::parallel_for(0, batchPointNum, [&](size_t i) { np[i] = nq[i]; });
        // writeToFile(one_uniform_nine_varden);
        read_points(one_uniform_nine_varden.c_str(), np, K);
        run();

        //@ 3: 1 varden, but flatten;
        // clean();
        // path = prefix + "/1.in";
        // // std::cout << path << std::endl;
        // read_points<point>(path.c_str(), np, K);
        // buildTree<point, 0>(Dim, np, rounds, pkd);
        // pkd.flatten(pkd.get_root(), parlay::make_slice(np));
        // run();

        // delete[] kdknn;
    }

    if (queryType & (1 << 11)) {  // NOTE: osm by year
        // WARN: remember using double
        string osm_prefix = "/data/zmen002/kdtree/real_world/osm/year/";
        const std::vector<std::string> files = {"2014", "2015", "2016", "2017", "2018",
                                                "2019", "2020", "2021", "2022", "2023"};
        parlay::sequence<points> node_by_year(files.size());
        for (int i = 0; i < files.size(); i++) {
            std::string path = osm_prefix + "osm_" + files[i] + ".csv";
            // LOG << path << ENDL;
            read_points(path.c_str(), node_by_year[i], K);
        }
        kdknn = new Typename[batchQueryOsmSize];
        insertOsmByTime<point>(Dim, node_by_year, rounds, pkd, K, kdknn);
        delete[] kdknn;

        // auto all_points = parlay::flatten(node_by_year);
        // queryKNN<point>(Dim, all_points, rounds, pkd, kdknn, K, false);
    }

    if (queryType & (1 << 12)) {  // NOTE: osm by month
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
        kdknn = new Typename[batchQueryOsmSize];
        insertOsmByTime<point>(Dim, node, rounds, pkd, K, kdknn);
        delete[] kdknn;
        // auto all_points = parlay::flatten(node);
        // queryKNN<point>(Dim, all_points, rounds, pkd, kdknn, K, false);
    }

    if (queryType & (1 << 13)) {  // NOTE: serial insert VS batch insert
        // NOTE: first insert in serial one bu one
        const parlay::sequence<double> ratios = {1e-9, 2e-9, 5e-9, 1e-8, 2e-8, 5e-8, 1e-7, 2e-7, 5e-7, 1e-6, 2e-6,
                                                 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3, 1e-2};
        // LOG << ENDL << "serial ";
        // batchInsert<point, true>(pkd, wp, wi, Dim, rounds, *ratios.rbegin());
        LOG << ENDL;
        for (int i = 0; i < ratios.size(); i++) {
            LOG << wi.size() * ratios[i] << " ";
            batchUpdateByStep<point, true>(pkd, wp, wi, Dim, rounds, ratios[i], *ratios.rbegin());
            LOG << ENDL;
        }
    }

    if (queryType & (1 << 14)) {  // NOTE: serial delete VS batch delete
        // NOTE: first insert in serial one bu one
        const parlay::sequence<double> ratios = {1e-9, 2e-9, 5e-9, 1e-8, 2e-8, 5e-8, 1e-7, 2e-7, 5e-7, 1e-6, 2e-6,
                                                 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3, 1e-2};
        // LOG << ENDL << "serial ";
        // batchDelete<point, true>(pkd, wp, wi, Dim, rounds, false, *ratios.rbegin());
        // LOG << ENDL;
        for (int i = 0; i < ratios.size(); i++) {
            LOG << wi.size() * ratios[i] << " ";
            batchUpdateByStep<point, false>(pkd, wp, wp, Dim, rounds, ratios[i], *ratios.rbegin());
            LOG << ENDL;
        }
    }

    std::cout << std::endl << std::flush;

    pkd.delete_tree();

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

    int LEAVE_WRAP = 32;
    parlay::sequence<PointType<coord, 15>> wp;
    // parlay::sequence<PointID<coord, 15>> wp;
    std::string name, insertFile = "";

    //* initialize points
    if (iFile != NULL) {
        name = std::string(iFile);
        name = name.substr(name.rfind("/") + 1);
        std::cout << name << " ";
        auto [n, d] = read_points<PointType<coord, 15>>(iFile, wp, K);
        // auto [n, d] = read_points<PointID<coord, 15>>( iFile, wp, K );
        N = n;
        assert(d == Dim);
    } else {  //* construct data byself
        K = 100;
        generate_random_points<PointType<coord, 15>>(wp, 1000000, N, Dim);
        // generate_random_points<PointID<coord, 15>>( wp, 1000000, N, Dim );
        std::string name = std::to_string(N) + "_" + std::to_string(Dim) + ".in";
        std::cout << name << " ";
    }

    int id = std::stoi(name.substr(0, name.find_first_of('.')));
    id = (id + 1) % 3;  //! MOD graph number used to test
    if (!id) id++;
    int pos = std::string(iFile).rfind("/") + 1;
    insertFile = std::string(iFile).substr(0, pos) + std::to_string(id) + ".in";

    assert(N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1);

    if (tag == -1) {
        //* serial run
        // todo rewrite test serial code
        // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    } else if (Dim == 2) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 2> { return PointType<coord, 2>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 2>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile, summary);
    } else if (Dim == 3) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 3> { return PointType<coord, 3>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 3>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile, summary);
    } else if (Dim == 5) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 5> { return PointType<coord, 5>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 5>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile, summary);
    } else if (Dim == 7) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 7> { return PointType<coord, 7>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 7>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile, summary);
    } else if (Dim == 9) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 9> { return PointType<coord, 9>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 9>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile, summary);
    } else if (Dim == 10) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 10> { return PointType<coord, 10>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 10>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                 readInsertFile, summary);
    }

    return 0;
}
