#include <algorithm>
#include <cstdlib>
#include "testFramework.h"

template<typename point>
void testParallelKDtree(const int& Dim, const int& LEAVE_WRAP,
                        parlay::sequence<point>& wp, const size_t& N,
                        const int& K, const int& rounds,
                        const string& insertFile, const int& tag,
                        const int& queryType, const int read_insert_file) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using interior = typename tree::interior;
    using leaf = typename tree::leaf;
    using node_tag = typename tree::node_tag;
    using node_tags = typename tree::node_tags;
    using box = typename tree::box;
    using boxs = parlay::sequence<box>;

    tree pkd;

    Typename* kdknn = nullptr;

    // NOTE: queryType == 0: only read
    // 0001: build tree only, the input will be poisoned
    // 0011: build tree and keep input clean
    // 0111: build tree and generate boxes
    // 1111: build tree and generate boxes and do range query
    if (queryType & (1 << 0)) {
        LOG << "build tree" << ENDL;
        buildTree<point>(Dim, wp, rounds, pkd, queryType);

        // double loopLate = rounds > 1 ? 1.0 : -0.1;
        if (queryType & (1 << 2)) { // NOTE: range query
            LOG << "gen box" << ENDL;
            size_t recNum = realworldRangeQueryNum;
            kdknn = new Typename[recNum];
            points Out;
            auto [queryBox, maxSize] = gen_rectangles(recNum, 2, wp, Dim);
            Out.resize(recNum * maxSize);
            int n = wp.size();
            using ref_t = std::reference_wrapper<point>;
            parlay::sequence<ref_t> out_ref(Out.size(), std::ref(Out[0]));

            if (queryType & (1 << 3)) {
                LOG << "range query" << ENDL;
                rangeQueryFix<point>(wp, pkd, kdknn, rounds, Out, 2, recNum,
                                     Dim, queryBox);
            }
            // delete[] kdknn;
        }

        std::cout << std::endl << std::flush;
    }

    // pkd.delete_tree();

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
    perf_ctl_fd = P.getOptionIntValue("-pcf", 0);
    perf_ctl_ack_fd = P.getOptionIntValue("-pcaf", 0);
    printf("perf control: %d %d\n", perf_ctl_fd, perf_ctl_ack_fd);
    int LEAVE_WRAP = 32;
    // using point = PointType<coord, 3>;
    // parlay::sequence<point> wp;
    // parlay::sequence<PointID<coord, 15>> wp;
    std::string name, insertFile = "";

    //* initialize points
    if (iFile != NULL) {
        name = std::string(iFile);
        name = name.substr(name.rfind("/") + 1);
        std::cout << name << " ";
    }

    if (readInsertFile == 1) {
        int id = std::stoi(name.substr(0, name.find_first_of('.')));
        id = (id + 1) % 3; //! MOD graph number used to test
        if (!id) id++;
        int pos = std::string(iFile).rfind("/") + 1;
        insertFile =
            std::string(iFile).substr(0, pos) + std::to_string(id) + ".in";
    }

    assert(N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1);
    // testParallelKDtree<point>(Dim, LEAVE_WRAP, wp, N, K, rounds, insertFile,
    // tag, queryType, readInsertFile);

    if (tag == -1) {
        //* serial run
        // todo rewrite test serial code
        // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    } else if (Dim == 2) {
        parlay::sequence<PointType<coord, 2>> wp;
        auto [n, d] = read_points<PointType<coord, 2>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 2>>(Dim, LEAVE_WRAP, wp, N, K,
                                                rounds, insertFile, tag,
                                                queryType, readInsertFile);
    } else if (Dim == 3) {
        parlay::sequence<PointType<coord, 3>> wp;
        auto [n, d] = read_points<PointType<coord, 3>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 3>>(Dim, LEAVE_WRAP, wp, N, K,
                                                rounds, insertFile, tag,
                                                queryType, readInsertFile);
    } else if (Dim == 5) {
        parlay::sequence<PointType<coord, 5>> wp;
        auto [n, d] = read_points<PointType<coord, 5>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 5>>(Dim, LEAVE_WRAP, wp, N, K,
                                                rounds, insertFile, tag,
                                                queryType, readInsertFile);
    } else if (Dim == 7) {
        parlay::sequence<PointType<coord, 7>> wp;
        auto [n, d] = read_points<PointType<coord, 7>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 7>>(Dim, LEAVE_WRAP, wp, N, K,
                                                rounds, insertFile, tag,
                                                queryType, readInsertFile);
    } else if (Dim == 9) {
        parlay::sequence<PointType<coord, 9>> wp;
        auto [n, d] = read_points<PointType<coord, 9>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 9>>(Dim, LEAVE_WRAP, wp, N, K,
                                                rounds, insertFile, tag,
                                                queryType, readInsertFile);
    } else if (Dim == 10) {
        parlay::sequence<PointType<coord, 10>> wp;
        auto [n, d] = read_points<PointType<coord, 10>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 10>>(Dim, LEAVE_WRAP, wp, N, K,
                                                 rounds, insertFile, tag,
                                                 queryType, readInsertFile);
    } else if (Dim == 16) {
        parlay::sequence<PointType<coord, 16>> wp;
        auto [n, d] = read_points<PointType<coord, 16>>(iFile, wp, K);
        N = n;
        testParallelKDtree<PointType<coord, 16>>(Dim, LEAVE_WRAP, wp, N, K,
                                                 rounds, insertFile, tag,
                                                 queryType, readInsertFile);
    }

    return 0;
}
