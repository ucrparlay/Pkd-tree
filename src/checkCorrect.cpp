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
   KDnode<double>* KDroot = KD.init( Dim, 16, wp, N );

   //* query phase
   std::random_shuffle( wp, wp + N );
   double* cgknn = new double[N];
   double* kdknn = new double[N];
   K = 100;
   assert( N >= K );
   //* cgal query
   for( int i = 0; i < N; i++ )
   {
      Point_d query( Dim, std::begin( wp[i].x ), std::begin( wp[i].x ) + Dim );
      Neighbor_search search( tree, query, K );
      Neighbor_search::iterator it = search.end();
      it--;
      cgknn[i] = std::sqrt( it->second );
   }
   //* kd query
   for( int i = 0; i < N; i++ )
   {
      double ans = KD.query_k_nearest( &wp[i], K );
      kdknn[i] = std::sqrt( ans );
   }

   //* karray
   // kArrayQueue<double>* kq = new kArrayQueue<double>[N];
   // for( int i = 0; i < N; i++ )
   // {
   //    kq[i].resize( K );
   // }
   // parlay::parallel_for( 0, N,
   //                       [&]( size_t i )
   //                       {
   //                          KD.k_nearest_array( KDroot, &wp[i], 0, kq[i] );
   //                          kdknn[i] = std::sqrt( kq[i].queryKthElement() );
   //                       } );

   //* bounded_queue
   // kBoundedQueue<double>* bq = new kBoundedQueue<double>[N];
   // for( int i = 0; i < N; i++ )
   // {
   //    bq[i].resize( K );
   // }
   // parlay::parallel_for( 0, N,
   //                       [&]( size_t i )
   //                       {
   //                          KD.k_nearest( KDroot, &wp[i], 0, bq[i] );
   //                          kdknn[i] = std::sqrt( bq[i].top() );
   //                       } );

   //* verify
   parlay::parallel_for( 0, N,
                         [&]( size_t i )
                         {
                            if( std::abs( cgknn[i] - kdknn[i] ) > 1e-4 )
                            {
                               puts( "wrong" );
                               exit( 1 );
                            }
                         } );
   puts( "ok" );
   return 0;
}