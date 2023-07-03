#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <bits/stdc++.h>
#include <iterator>

typedef CGAL::Cartesian_d<long long> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
typedef Neighbor_search::Tree Tree;

int N, Dim, Q, K;

const size_t MAX_DIM = 15;
const size_t MAXN = 2e6 + 5;
const long long eps = 1e-10;

struct kd_node_t {
   long long x[MAX_DIM];
   struct kd_node_t *left, *right;
   int num;                // number of nodes in subtree plus itself
   long long mxx[MAX_DIM]; // mxx[i] the maximum of sub points on dimension i
   long long mnx[MAX_DIM]; // mnx[i] the minimum
};
kd_node_t* wp;

void
setup( char* path ) {
   freopen( path, "r", stdin );

   scanf( "%d %d", &N, &Dim );
   wp = new kd_node_t[N];
   int i, j;

   for( i = 0; i < N; i++ ) {
      for( j = 0; j < Dim; j++ ) {
         scanf( "%lld", &wp[i].x[j] );
      }
   }
}

int
main( int argc, char* argv[] ) {
   assert( argc >= 2 );
   int K = 100;
   std::string name( argv[1] );
   if( argc >= 3 )
      K = std::stoi( argv[2] );

   // std::cout.precision( 5 );
   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   CGAL::Timer timer;
   setup( argv[1] );
   assert( N >= K );

   timer.start();
   std::list<Point_d> points;
   for( int i = 0; i < N; i++ ) {
      //   printf( "%.3Lf\n", *( std::begin( wp[i].x ) + Dim - 1 ) );
      points.push_back( Point_d( Dim, std::begin( wp[i].x ),
                                 ( std::begin( wp[i].x ) + Dim ) ) );
   }

   Tree tree( points.begin(), points.end() );
   timer.stop();
   std::cout << std::fixed << timer.time() << " ";
   timer.reset();

   //* start test
   std::random_shuffle( wp, wp + N );

   timer.start();
   for( int i = 0; i < N; i++ ) {
      Point_d query( Dim, wp[i].x, wp[i].x + Dim );
      Neighbor_search search( tree, query, K );

      // Neighbor_search::iterator it = search.end();
      // it--;
      // printf( "%.6lf\n", std::sqrt( it->second ) );

      // for( Neighbor_search::iterator it = search.begin(); it != search.end();
      //      ++it )
      //    std::cout << it->first << " " << std::sqrt( it->second ) <<
      //    std::endl;
      // puts( "" );
   }
   timer.stop();
   std::cout << std::fixed << timer.time() << " -1 " << std::endl;

   return 0;
}