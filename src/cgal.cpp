#include "kdTree.h"
#include "kdTreeParallel.h"

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

using Typename = coord;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Euclidean_distance<TreeTraits> Distance;

//@ median tree
typedef CGAL::Median_of_rectangle<TreeTraits> Median_of_rectangle;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance,
                                           Median_of_rectangle>
    Neighbor_search_Median;
typedef Neighbor_search_Median::Tree Tree_Median;

//@ midpoint tree
typedef CGAL::Midpoint_of_rectangle<TreeTraits> Midpoint_of_rectangle;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits, Distance,
                                           Midpoint_of_rectangle>
    Neighbor_search_Midpoint;
typedef Neighbor_search_Midpoint::Tree Tree_Midpoint;

//@ begin function
template <typename Splitter, typename Tree, typename Neighbor_search>
void
testCGALSerial( int Dim, int LEAVE_WRAP, points wp, int N, int K ) {
   parlay::internal::timer timer;

   //* cgal
   std::vector<Point_d> _points( N );
   parlay::parallel_for(
       0, N,
       [&]( size_t i ) {
          _points[i] = Point_d( Dim, std::begin( wp[i].pnt ),
                                ( std::begin( wp[i].pnt ) + Dim ) );
       },
       FOR_BLOCK_SIZE );

   timer.start();
   Splitter split;
   Tree tree( _points.begin(), _points.end(), split );
   tree.build();
   timer.stop();

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( wp.cut( 0, N ) );
   Typename* cgknn = new Typename[N];

   timer.reset();
   timer.start();
   for( int i = 0; i < N; i++ ) {
      Point_d query( Dim, std::begin( wp[i].pnt ),
                     std::begin( wp[i].pnt ) + Dim );
      Neighbor_search search( tree, query, K );
      auto it = search.end();
      it--;
      cgknn[i] = it->second;
   }

   timer.stop();
   std::cout << timer.total_time() << " " << -1 << " " << -1 << std::endl
             << std::flush;

   // std::list<Point_d>().swap( _points );
   // points().swap( wp );
   // tree.clear();
   // delete[] cgknn;

   return;
}

template <typename Splitter, typename Tree, typename Neighbor_search>
void
testCGALParallel( int Dim, int LEAVE_WRAP, points wp, int N, int K ) {
   parlay::internal::timer timer;
   puts( "here" );
   //* cgal
   std::vector<Point_d> _points( N );
   parlay::parallel_for(
       0, N,
       [&]( size_t i ) {
          _points[i] = Point_d( Dim, std::begin( wp[i].pnt ),
                                ( std::begin( wp[i].pnt ) + Dim ) );
       },
       FOR_BLOCK_SIZE );

   timer.start();
   Splitter split;
   Tree tree( _points.begin(), _points.end(), split );
   tree.template build<CGAL::Parallel_tag>();
   timer.stop();
   std::vector<Point_d>().swap( _points );

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( wp.cut( 0, N ) );
   Typename* cgknn = new Typename[N];

   timer.reset();
   timer.start();

   tbb::parallel_for( tbb::blocked_range<std::size_t>( 0, _points.size() ),
                      [&]( const tbb::blocked_range<std::size_t>& r ) {
                         for( std::size_t s = r.begin(); s != r.end(); ++s ) {
                            // Neighbor search can be instantiated from
                            // several threads at the same time
                            Point_d query( Dim, std::begin( wp[s].pnt ),
                                           std::begin( wp[s].pnt ) + Dim );
                            Neighbor_search search( tree, query, K );
                            auto it = search.end();
                            it--;
                            cgknn[s] = it->second;
                         }
                      } );

   timer.stop();
   std::cout << timer.total_time() << " " << -1 << " " << -1 << std::endl
             << std::flush;

   // std::list<Point_d>().swap( _points );
   // points().swap( wp );
   // tree.clear();
   // delete[] cgknn;

   return;
}

int
main( int argc, char* argv[] ) {
   assert( argc >= 2 );

   int K = 100, LEAVE_WRAP = 16, Dim = -1;
   long N = -1;
   points wp;
   std::string name( argv[1] );

   //* initialize points
   if( name.find( "/" ) != std::string::npos ) { //* read from file
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";
      auto f = freopen( argv[1], "r", stdin );
      if( argc >= 3 )
         K = std::stoi( argv[2] );
      assert( f != nullptr );

      scanf( "%ld %d", &N, &Dim );
      assert( N >= K );
      wp.resize( N );

      for( int i = 0; i < N; i++ ) {
         for( int j = 0; j < Dim; j++ ) {
            scanf( "%ld", &wp[i].pnt[j] );
         }
      }
   } else { //* construct data byself
      K = 100;
      coord box_size = 10000000;

      std::random_device rd;       // a seed source for the random number engine
      std::mt19937 gen_mt( rd() ); // mersenne_twister_engine seeded with rd()
      std::uniform_int_distribution<int> distrib( 1, box_size );

      parlay::random_generator gen( distrib( gen_mt ) );
      std::uniform_int_distribution<int> dis( 0, box_size );

      long n = std::stoi( argv[1] );
      N = n;
      Dim = std::stoi( argv[2] );
      wp.resize( N );
      // generate n random points in a cube
      parlay::parallel_for(
          0, n,
          [&]( long i ) {
             auto r = gen[i];
             for( int j = 0; j < Dim; j++ ) {
                wp[i].pnt[j] = dis( r );
             }
          },
          1000 );
      name = std::to_string( n ) + "_" + std::to_string( Dim ) + ".in";
      std::cout << name << " ";
   }

   // if( argc >= 4 )
   //    K = std::stoi( argv[3] );
   // if( argc >= 5 )
   //    LEAVE_WRAP = std::stoi( argv[4] );

   assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );
   if( argc >= 4 ) {
      if( std::stoi( argv[3] ) == 1 )
         testCGALParallel<Median_of_rectangle, Tree_Median,
                          Neighbor_search_Median>( Dim, LEAVE_WRAP, wp, N, K );
      else if( std::stoi( argv[3] ) == 0 )
         testCGALSerial<Median_of_rectangle, Tree_Median,
                        Neighbor_search_Median>( Dim, LEAVE_WRAP, wp, N, K );
   }

   return 0;
}