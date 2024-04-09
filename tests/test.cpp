#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include "testFramework.h"

template<typename point>
void testParallelKDtree(const int& Dim, const int& LEAVE_WRAP, parlay::sequence<point>& wp, const size_t& N,
                        const int& K, const int& rounds, const string& insertFile, const int& tag, const int& queryType,
                        const int readInsertFile) {
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
        const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
        for (int i = 0; i < ratios.size(); i++) {
            batchInsert<point>(pkd, wp, wi, Dim, rounds, batchInsertRatio);
        }
    }

    //* batch delete
    if (tag >= 2) {
        const parlay::sequence<double> ratios = {0.0001, 0.001, 0.01, 0.1};
        for (int i = 0; i < ratios.size(); i++) {
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

        if (tag == 0) {
            int k[3] = {1, 10, 100};
            for (int i = 0; i < 3; i++) {
                run_batch_knn(wp, k[i], batchSize);
            }
        } else {  // test summary
            run_batch_knn(wp, K, batchSize);
        }
        delete[] kdknn;
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

    int recNum = rangeQueryNum;

    if (queryType & (1 << 2)) {  // NOTE: range count
        kdknn = new Typename[recNum];
        const int type[3] = {0, 1, 2};

        for (int i = 0; i < 3; i++) {
            // rangeCountFix<point>(wp, pkd, kdknn, rounds, type[i], recNum, Dim);
            rangeCountFixWithLog<point>(wp, pkd, kdknn, rounds, type[i], recNum, Dim);
        }
        //
        // rangeCountFix<point>(wp, pkd, kdknn, rounds, 2, rangeQueryNumInbaRatio, Dim);

        delete[] kdknn;
    }

    if (queryType & (1 << 3)) {  // NOTE: range query
        if (tag == 0) {
            const int type[3] = {0, 1, 2};
            for (int i = 0; i < 3; i++) {
                //* run range count to obtain size
                kdknn = new Typename[recNum];
                points Out;
                rangeQuerySerialWithLog<point>(wp, pkd, kdknn, rounds, Out, type[i], recNum, Dim);
            }
        } else if (tag == 2) {  // NOTE: for summary
            kdknn = new Typename[summaryRangeQueryNum];
            points Out;
            rangeQueryFix<point>(wp, pkd, kdknn, rounds, Out, 2, summaryRangeQueryNum, Dim);
        }

        delete[] kdknn;
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

        points np, nq;
        std::string prefix, path;
        const string insertFileBack = insertFile;

        auto inbaQueryType = std::stoi(std::getenv("INBA_QUERY"));
        auto inbaBuildType = std::stoi(std::getenv("INBA_BUILD"));

        // NOTE: helper functions
        auto clean = [&]() {
            prefix = insertFile.substr(0, insertFile.rfind("/"));
            // prefix = insertFileBack.substr(0, insertFileBack.rfind("/"));
            // LOG << insertFile << " " << insertFileBack << " " << prefix << ENDL;
            np.clear();
            nq.clear();
        };

        // NOTE: run the test
        auto run = [&]() {
            if (inbaBuildType == 0) {
                buildTree<point, 2>(Dim, np, rounds, pkd);
            } else {
                incrementalBuild<point, 2>(Dim, np, rounds, pkd, insertBatchInbaRatio);
            }

            if (inbaQueryType == 0) {
                size_t batchSize = static_cast<size_t>(np.size() * knnBatchInbaRatio);
                points newPts(batchSize);
                parlay::copy(np.cut(0, batchSize), newPts.cut(0, batchSize));
                kdknn = new Typename[batchSize];
                const int k[3] = {1, 5, 100};
                for (int i = 0; i < 3; i++) {
                    queryKNN<point, 0, 1>(Dim, newPts, rounds, pkd, kdknn, k[i], true);
                }
                delete[] kdknn;
            } else if (inbaQueryType == 1) {
                kdknn = new Typename[rangeQueryNumInbaRatio];
                int type = 2;
                rangeCountFix<point>(np, pkd, kdknn, rounds, type, rangeQueryNumInbaRatio, Dim);
                delete[] kdknn;
            }
        };

        LOG << "alpha: " << pkd.get_imbalance_ratio() << ENDL;
        // HACK: need start with varden file
        // NOTE: 1: 10*0.1 different vardens.
        clean();
        for (int i = 1; i <= fileNum; i++) {
            path = prefix + "/" + std::to_string(i) + ".in";
            // std::cout << path << std::endl;
            read_points<point>(path.c_str(), nq, K);
            np.append(nq.cut(0, batchPointNum));
            nq.clear();
        }
        assert(np.size() == wp.size());
        run();

        //@ 2: 1 uniform, and 9*0.1 same varden
        //* read varden first
        clean();
        path = prefix + "/1.in";
        // std::cout << "varden path" << path << std::endl;
        read_points<point>(path.c_str(), np, K);
        //* then read uniforprefixm
        prefix = prefix.substr(0, prefix.rfind("/"));  // 1000000_3
        prefix = prefix.substr(0, prefix.rfind("/"));  // ss_varden
        path = prefix + "/uniform/" + std::to_string(wp.size()) + "_" + std::to_string(Dim) + "/1.in";
        // std::cout << "uniform path:" << path << std::endl;

        read_points<point>(path.c_str(), nq, K);
        parlay::parallel_for(0, batchPointNum, [&](size_t i) { np[i] = nq[i]; });
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

    // generate_knn( Dim, wp, K, "knn.out" );

    std::cout << std::endl << std::flush;

    pkd.delete_tree();

    return;
}

int main(int argc, char* argv[]) {
    commandLine P(argc, argv,
                  "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
                  "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i "
                  "<_insertFile>]");
    char* iFile = P.getOptionValue("-p");
    int K = P.getOptionIntValue("-k", 100);
    int Dim = P.getOptionIntValue("-d", 3);
    size_t N = P.getOptionLongValue("-n", -1);
    int tag = P.getOptionIntValue("-t", 1);
    int rounds = P.getOptionIntValue("-r", 3);
    int queryType = P.getOptionIntValue("-q", 0);
    int readInsertFile = P.getOptionIntValue("-i", 1);

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

    // if ( tag == -1 ) {
    //   //* serial run
    //   // todo rewrite test serial code
    //   // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    // } else if ( Dim == 2 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 2> {
    //     return PointID<coord, 2>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 2>>( Dim, LEAVE_WRAP, pts, N, K,
    //   rounds, insertFile,
    //                                          tag, queryType );
    // } else if ( Dim == 3 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 3> {
    //     return PointID<coord, 3>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 3>>( Dim, LEAVE_WRAP, pts, N, K,
    //   rounds, insertFile,
    //                                          tag, queryType );
    // } else if ( Dim == 5 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 5> {
    //     return PointID<coord, 5>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 5>>( Dim, LEAVE_WRAP, pts, N, K,
    //   rounds, insertFile,
    //                                          tag, queryType );
    // } else if ( Dim == 7 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 7> {
    //     return PointID<coord, 7>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 7>>( Dim, LEAVE_WRAP, pts, N, K,
    //   rounds, insertFile,
    //                                          tag, queryType );
    // }
    //
    if (tag == -1) {
        //* serial run
        // todo rewrite test serial code
        // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    } else if (Dim == 2) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 2> { return PointType<coord, 2>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 2>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile);
    } else if (Dim == 3) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 3> { return PointType<coord, 3>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 3>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile);
    } else if (Dim == 5) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 5> { return PointType<coord, 5>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 5>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile);
    } else if (Dim == 7) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 7> { return PointType<coord, 7>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 7>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile);
    } else if (Dim == 9) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 9> { return PointType<coord, 9>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 9>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                readInsertFile);
    } else if (Dim == 10) {
        auto pts = parlay::tabulate(
            N, [&](size_t i) -> PointType<coord, 10> { return PointType<coord, 10>(wp[i].pnt.begin()); });
        decltype(wp)().swap(wp);
        testParallelKDtree<PointType<coord, 10>>(Dim, LEAVE_WRAP, pts, N, K, rounds, insertFile, tag, queryType,
                                                 readInsertFile);
    }

    return 0;
}
