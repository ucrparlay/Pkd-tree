#include <iostream>
#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

int main() {
    // Define a 2D point type using Boost.Geometry
    typedef bg::model::point<float, 2, bg::cs::cartesian> Point;
    typedef std::pair<Point, unsigned> Value;

    // Create an R-tree, which stores pairs of points and their associated values (unsigned in this case)
    bgi::rtree<Value, bgi::quadratic<16>> rtree;

    // Insert some points into the R-tree
    rtree.insert(std::make_pair(Point(0, 0), 1));
    rtree.insert(std::make_pair(Point(1, 1), 2));
    rtree.insert(std::make_pair(Point(2, 2), 3));
    rtree.insert(std::make_pair(Point(3, 3), 4));
    rtree.insert(std::make_pair(Point(4, 4), 5));

    // Define a query point and a search radius
    Point query_point(2.5f, 2.5f);
    float search_radius = 2.0f;

    // Define a box (bounding box) around the query point with the search radius
    Point lower_corner(query_point.get<0>() - search_radius, query_point.get<1>() - search_radius);
    Point upper_corner(query_point.get<0>() + search_radius, query_point.get<1>() + search_radius);
    bg::model::box<Point> query_box(lower_corner, upper_corner);

    // Perform a query to find all points within the search radius
    std::vector<Value> results;
    rtree.query(bgi::intersects(query_box), std::back_inserter(results));

    // Output the results
    std::cout << "Points within " << search_radius << " units of (" << query_point.get<0>() << ", "
              << query_point.get<1>() << "):" << std::endl;
    for (const auto& result : results) {
        std::cout << "Point(" << result.first.get<0>() << ", " << result.first.get<1>() << ") with value "
                  << result.second << std::endl;
    }

    return 0;
}
