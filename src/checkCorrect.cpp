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

using Typename = long long;

typedef CGAL::Cartesian_d<Typename> Kernel;
typedef Kernel::Point_d Point_d;
typedef CGAL::Search_traits_d<Kernel> TreeTraits;
typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
typedef Neighbor_search::Tree Tree;

int Dim, Q, K;
long N;

int
main( int argc, char* argv[] )
{
   std::cout.precision( 5 );
   Point<Typename>* wp;
   std::string name( argv[1] );

   if( name.find( "/" ) != std::string::npos )
   {
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";

      freopen( argv[1], "r", stdin );

      scanf( "%ld %d", &N, &Dim );
      wp = new Point<Typename>[N];
      for( int i = 0; i < N; i++ )
      {
         for( int j = 0; j < Dim; j++ )
         {
            scanf( "%lf", &wp[i].x[j] );
         }
      }
   }
   else
   {
      parlay::random_generator gen( 0 );
      int box_size = 1000000000;
      std::uniform_int_distribution<int> dis( 0, box_size );
      assert( argc >= 3 );
      long n = std::stoi( argv[1] );
      N = n;
      Dim = std::stoi( argv[2] );
      wp = new Point<Typename>[n];
      // generate n random points in a cube
      parlay::parallel_for( 0, n,
                            [&]( long i )
                            {
                               auto r = gen[i];
                               for( int j = 0; j < Dim; j++ )
                               {
                                  wp[i].x[j] = dis( r );
                               }
                            } );
   }

   //* cgal
   std::list<Point_d> points;
   for( long i = 0; i < N; i++ )
   {
      //   printf( "%.3Lf\n", *( std::begin( wp[i].x ) + _Dim - 1 ) );
      points.push_back( Point_d( Dim, std::begin( wp[i].x ),
                                 ( std::begin( wp[i].x ) + Dim ) ) );
   }
   Tree tree( points.begin(), points.end() );

   //* kd tree
   KDtree<Typename> KD;
   KDnode<Typename>* KDroot = KD.init( Dim, 16, wp, N );

   //* query phase
   std::random_shuffle( wp, wp + N );
   Typename* cgknn = new Typename[N];
   Typename* kdknn = new Typename[N];
   K = 100;
   assert( N >= K );
   //* cgal query
   for( int i = 0; i < N; i++ )
   {
      Point_d query( Dim, std::begin( wp[i].x ), std::begin( wp[i].x ) + Dim );
      Neighbor_search search( tree, query, K );
      Neighbor_search::iterator it = search.end();
      it--;
      // std::cout << i << " " << it->second << std::endl;
      cgknn[i] = it->second;
   }
   //* kd query

   // for( int i = 0; i < N; i++ )
   // {
   //    Typename ans = KD.query_k_nearest( &wp[i], K );
   //    kdknn[i] = ans;
   // }

   //* karray
   // kArrayQueue<Typename>* kq = new kArrayQueue<Typename>[N];
   // for( int i = 0; i < N; i++ )
   // {
   //    kq[i].resize( K );
   // }
   // parlay::parallel_for( 0, N,
   //                       [&]( size_t i )
   //                       {
   //                          KD.k_nearest_array( KDroot, &wp[i], 0, kq[i]
   //                          ); kdknn[i] = std::sqrt(
   //                          kq[i].queryKthElement() );
   //                       } );

   //* bounded_queue
   kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   for( int i = 0; i < N; i++ )
   {
      bq[i].resize( K );
   }
   parlay::parallel_for( 0, N,
                         [&]( size_t i )
                         {
                            KD.k_nearest( KDroot, &wp[i], 0, bq[i] );
                            kdknn[i] = bq[i].top();
                         } );

   //* verify
   parlay::parallel_for( 0, N,
                         [&]( size_t i )
                         {
                            if( std::abs( cgknn[i] - kdknn[i] ) > 1e-4 )
                            {
                               std::cout << i << " " << cgknn[i] << " "
                                         << kdknn[i] << std::endl;
                               puts( "wrong" );
                               exit( 1 );
                            }
                         } );
   puts( "ok" );
   return 0;
}