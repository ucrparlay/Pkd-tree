#include "common/parse_command_line.h"

#include "cpdd/basic_point.h"
#include "parlay/slice.h"
#include "testFramework.h"
#include <fstream>

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Fuzzy_sphere.h>
#include <string>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

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
void testCGALParallel(int Dim, int LEAVE_WRAP, parlay::sequence<point>& wp, std::vector<Point_d>& _points, int N, int K,
                      const int& rounds, const string& insertFile, const int& tag, const int& queryType,
                      const int readInsertFile) {
    using points = parlay::sequence<point>;
    using pkdTree = ParallelKDtree<point>;
    using box = typename pkdTree::box;

    parlay::internal::timer timer;

    points wi;
    N = wp.size();

    // return;
    timer.start();
    Splitter split;
    Tree tree(_points.begin(), _points.end(), split);
    tree.template build<CGAL::Parallel_tag>();
    timer.stop();

    std::cout << timer.total_time() << " " << tree.root()->depth() << " " << std::flush;

    // return;
    // copy the wp, increase the memory usage
    wp.resize(_points.size());
    parlay::parallel_for(0, _points.size(), [&](size_t i) {
        for (int j = 0; j < Dim; j++) {
            wp[i].pnt[j] = _points[i].cartesian(j);
        }
    });
    Typename* cgknn;
    if (queryType & (1 << 3)) { // NOTE: range query
        auto run_cgal_range_query = [&](int type, size_t queryNum) {
            size_t n = wp.size();
            auto [queryBox, maxSize] = gen_rectangles<point>(queryNum, type, wp, Dim);
            LOG << queryNum << " " << maxSize << " " << std::flush;
            // using ref_t = std::reference_wrapper<Point_d>;
            // std::vector<ref_t> out_ref( queryNum * maxSize, std::ref( _points[0] )
            // );
            std::vector<Point_d> _ans(queryNum * maxSize);

            timer.reset();
            timer.start();

            tbb::parallel_for(
                tbb::blocked_range<std::size_t>(0, queryNum), [&](const tbb::blocked_range<std::size_t>& r) {
                    for (std::size_t s = r.begin(); s != r.end(); ++s) {
                        Point_d a(Dim, std::begin(queryBox[s].first.first.pnt), std::end(queryBox[s].first.first.pnt)),
                            b(Dim, std::begin(queryBox[s].first.second.pnt), std::end(queryBox[s].first.second.pnt));
                        Fuzzy_iso_box fib(a, b, 0.0);
                        auto it = tree.search(_ans.begin() + s * maxSize, fib);
                        cgknn[s] = std::distance(_ans.begin() + s * maxSize, it);
                    }
                });

            timer.stop();
        };

        cgknn = new Typename[realworldRangeQueryNum];
        run_cgal_range_query(2, realworldRangeQueryNum);
    }

    std::cout << std::endl << std::flush;

    return;
}

template<typename point>
std::pair<size_t, int> cgal_read_points(const char* iFile, std::vector<Point_d>& wp, int K, bool withID = false) {
    using coord = typename point::coord;
    using coords = typename point::coords;
    static coords samplePoint;

    ifstream fs;
    fs.open(iFile);
    size_t N;
    int Dim;
    string str;
    fs >> str, N = stol(str);
    fs >> str, Dim = stoi(str);
    // LOG << N << " " << Dim << ENDL;
    wp.resize(N);
    for (size_t i = 0; i < N; i++) {
        // std::array<coord, 3> arr;
        std::vector<coord> arr(Dim);
        for (int j = 0; j < Dim; j++) {
            fs >> str;
            arr[j] = stol(str);
        }
        wp[i] = Point_d(Dim, std::begin(arr), (std::begin(arr) + Dim));
    }

    return std::make_pair(N, Dim);
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

    using point = PointType<coord, 3>;
    using points = parlay::sequence<point>;

    // points wp;
    std::vector<Point_d> _points;
    std::string name, insertFile = "";
    int LEAVE_WRAP = 32;

    //* initialize points
    if (iFile != NULL) {
        name = std::string(iFile);
        name = name.substr(name.rfind("/") + 1);
        std::cout << name << " ";
        auto [n, d] = cgal_read_points<point>(iFile, _points, K);
        N = n;
        assert(Dim == d);
    }

    if (readInsertFile == 1) {
        int id = std::stoi(name.substr(0, name.find_first_of('.')));
        id = (id + 1) % 3; //! MOD graph number used to test
        if (!id) id++;
        int pos = std::string(iFile).rfind("/") + 1;
        insertFile = std::string(iFile).substr(0, pos) + std::to_string(id) + ".in";
    }

    assert(N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1);

    // return 0;
    // testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 3>>(
    //     Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    if (tag == -1) {
    } else if (Dim == 2) {
        parlay::sequence<PointType<coord, 2>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 2>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    } else if (Dim == 3) {
        parlay::sequence<PointType<coord, 3>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 3>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    } else if (Dim == 5) {
        parlay::sequence<PointType<coord, 5>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 5>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    } else if (Dim == 7) {
        parlay::sequence<PointType<coord, 7>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 7>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    } else if (Dim == 9) {
        parlay::sequence<PointType<coord, 9>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 9>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    } else if (Dim == 10) {
        parlay::sequence<PointType<coord, 10>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 10>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    } else if (Dim == 16) {
        parlay::sequence<PointType<coord, 16>> wp;
        testCGALParallel<Median_of_rectangle, Tree_Median, Neighbor_search_Median, PointType<coord, 16>>(
            Dim, LEAVE_WRAP, wp, _points, N, K, rounds, insertFile, tag, queryType, readInsertFile);
    }

    // else if ( tag == -1 )
    //   testCGALSerial<Median_of_rectangle, Tree_Median, Neighbor_search_Median>(
    //       Dim, LEAVE_WRAP, wp, N, K );

    return 0;
}
