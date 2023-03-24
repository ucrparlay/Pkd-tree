#include "kdTree.h"

#include <CGAL/Cartesian_d.h>
#include <CGAL/K_neighbor_search.h>
#include <CGAL/Kernel_d/Point_d.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_d.h>
#include <CGAL/Timer.h>
#include <CGAL/point_generators_d.h>
#include <bits/stdc++.h>
#include <iterator>

typedef CGAL::Cartesian_d<double> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
typedef Neighbor_search::Tree Tree;

struct kd_node_t *kdRoot, *kdWp;
int _N, _Dim, _Q, _K;

int
main( int argc, char* argv[] )
{
   std::cout.precision( 5 );
   std::string name( argv[1] );
   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   freopen( argv[1], "r", stdin );

   scanf( "%d %d", &_N, &_Dim );
   kdWp = (kd_node_t*)malloc( _N * sizeof( kd_node_t ) );

   for( int i = 0; i < _N; i++ )
   {
      for( int j = 0; j < _Dim; j++ )
      {
         scanf( "%lf", &kdWp[i].x[j] );
      }
   }

   //* cgal
   std::list<Point_d> points;
   for( int i = 0; i < _N; i++ )
   {
      //   printf( "%.3Lf\n", *( std::begin( wp[i].x ) + _Dim - 1 ) );
      points.push_back( Point_d( _Dim, std::begin( kdWp[i].x ),
                                 ( std::begin( kdWp[i].x ) + _Dim ) ) );
   }
   Tree tree( points.begin(), points.end() );

   //* kd
   kdRoot = make_tree( kdWp, _N, 0, _Dim );

   int i, j;
   scanf( "%d", &_Q );
   struct kd_node_t z;
   N = _N, Q = _Q, Dim = _Dim;
   while( _Q-- )
   {

      scanf( "%d", &_K );
      K = _K;
      for( j = 0; j < _Dim; j++ )
         scanf( "%lf", &z.x[j] );
      Point_d query( _Dim, std::begin( z.x ), std::begin( z.x ) + _Dim );
      Neighbor_search search( tree, query, _K );
      Neighbor_search::iterator it = search.end();
      it--;
      // std::cout << std::sqrt( it->second ) << std::endl;

      k_nearest( kdRoot, &z, 0, _Dim, _K );
      double dist = maxQueue();

      // printf( "%.8f %.8f\n", std::sqrt( it->second ), std::sqrt( dist ) );
      if( std::abs( std::sqrt( it->second ) - std::sqrt( dist ) ) > 1e-4 )
      {
         puts( "-1" );
         exit( 1 );
      }
      clearQueue();
   }
   puts( "ok" );
   return 0;
}