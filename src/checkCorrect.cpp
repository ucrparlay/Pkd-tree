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

int N, Dim, Q, K;

int
main( int argc, char* argv[] )
{
   std::cout.precision( 5 );
   std::string name( argv[1] );
   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   freopen( argv[1], "r", stdin );

   scanf( "%d %d", &N, &Dim );
   Point<double>* wp = new Point<double>[N];
   for( int i = 0; i < N; i++ )
   {
      for( int j = 0; j < Dim; j++ )
      {
         scanf( "%lf", &wp[i].x[j] );
      }
   }

   //* cgal
   std::list<Point_d> points;
   for( int i = 0; i < N; i++ )
   {
      //   printf( "%.3Lf\n", *( std::begin( wp[i].x ) + _Dim - 1 ) );
      points.push_back( Point_d( Dim, std::begin( wp[i].x ),
                                 ( std::begin( wp[i].x ) + Dim ) ) );
   }
   Tree tree( points.begin(), points.end() );

   //* kd tree
   KDtree<double> KD;
   KD.init( Dim, 16, wp, N );

   int i, j;
   scanf( "%d", &Q );
   Point<double> z;
   while( Q-- )
   {
      scanf( "%d", &K );
      for( j = 0; j < Dim; j++ )
         scanf( "%lf", &z.x[j] );
      Point_d query( Dim, std::begin( z.x ), std::begin( z.x ) + Dim );
      Neighbor_search search( tree, query, K );
      // for( auto it : search )
      // {
      //    printf( "%.2f ", it.second );
      // }
      Neighbor_search::iterator it = search.end();
      it--;
      // std::cout << std::sqrt( it->second ) << std::endl;

      // double dist = KD.query_k_nearest( &z, K );
      double dist = KD.query_k_nearest_array( &z, K );

      // printf( "%.8f %.8f\n", it->second, dist );
      if( std::abs( std::sqrt( it->second ) - std::sqrt( dist ) ) > 1e-4 )
      {
         puts( "-1" );
         exit( 1 );
      }
      // puts( "______" );
   }
   puts( "ok" );
   return 0;
}