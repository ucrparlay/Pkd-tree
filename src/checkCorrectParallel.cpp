#include "kdTree.h"
#include "kdTreeParallel.h"

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
   points wp;
   std::string name( argv[1] );

   if( name.find( "/" ) != std::string::npos )
   {
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";

      freopen( argv[1], "r", stdin );
      K = std::stoi( argv[2] );

      scanf( "%ld %d", &N, &Dim );
      wp.resize( N );
      for( int i = 0; i < N; i++ )
      {
         for( int j = 0; j < Dim; j++ )
         {
            scanf( "%lld", &wp[i].pnt[j] );
         }
      }
   }
   else
   {
      K = 100;

      parlay::random_generator gen( 0 );
      int box_size = 1000000000;
      std::uniform_int_distribution<int> dis( 0, box_size );
      assert( argc >= 3 );
      long n = std::stoi( argv[1] );
      N = n;
      Dim = std::stoi( argv[2] );
      wp.resize( N );
      // generate n random points in a cube
      parlay::parallel_for( 0, n,
                            [&]( long i )
                            {
                               auto r = gen[i];
                               for( int j = 0; j < Dim; j++ )
                               {
                                  wp[i].pnt[j] = dis( r );
                               }
                            } );
   }

   //* cgal
   std::list<Point_d> _points;
   for( long i = 0; i < N; i++ )
   {
      //   printf( "%.3Lf\n", *( std::begin( wp[i].x ) + _Dim - 1 ) );
      _points.push_back( Point_d( Dim, std::begin( wp[i].pnt ),
                                  ( std::begin( wp[i].pnt ) + Dim ) ) );
   }
   Tree tree( _points.begin(), _points.end() );

   //* query phase
   std::random_shuffle( wp.begin(), wp.begin() + N );
   Typename* cgknn = new Typename[N];
   Typename* kdknn = new Typename[N];
   assert( N >= K );

   //* cgal query
   for( int i = 0; i < N; i++ )
   {
      Point_d query( Dim, std::begin( wp[i].pnt ),
                     std::begin( wp[i].pnt ) + Dim );
      Neighbor_search search( tree, query, K );
      Neighbor_search::iterator it = search.end();
      it--;
      // std::cout << i << " " << it->second << std::endl;
      cgknn[i] = it->second;
   }

   //* begin kd tree
   std::cout << "kd tree" << std::endl;

   //* kd tree

   KDtree<Typename> KD;
   parlay::sequence<Point<Typename>> kdPoint;
   kdPoint.resize( N );
   parlay::parallel_for( 0, N,
                         [&]( size_t i )
                         {
                            for( int j = 0; j < Dim; j++ )
                            {
                               kdPoint[i].x[j] = wp[i].pnt[j];
                            }
                         } );
   KDnode<Typename>* KDroot = KD.init( Dim, 16, kdPoint, N );
   // node* KDParallelroot = build( wp.cut( 0, wp.size() ), 0, Dim );
   //* bounded_queue
   kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   for( int i = 0; i < N; i++ )
   {
      bq[i].resize( K );
   }
   // parlay::parallel_for( 0, N,
   //                       [&]( size_t i )
   //                       {
   //                          k_nearest( KDroot, wp[i], 0, Dim, bq[i] );
   //                          kdknn[i] = bq[i].top();
   //                       } );
   for( int i = 0; i < N; i++ )
   {
      // k_nearest( KDParallelroot, wp[i], 0, Dim, bq[i] );
      KD.k_nearest( KDroot, &kdPoint[i], 0, bq[i] );
      kdknn[i] = bq[i].top();
   }

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