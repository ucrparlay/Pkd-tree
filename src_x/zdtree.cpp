#include "../src/kdTreeParallel.h"
#include "zdtree/neighbors.h"

using points = parlay::sequence<point10D>;
using point2 = point2d<double>;
using point3 = point3d<double>;

template <class PT, int KK>
struct vertex {
   using pointT = PT;
   int identifier;
   pointT pt;       // the point itself
   vertex* ngh[KK]; // the list of neighbors
   vertex( pointT p, int id ) : pt( p ), identifier( id ) {}
   size_t counter;
   size_t counter2;
};

template <int maxK, class point>
void
testZdtree( parlay::sequence<point>& pts, const int k, const int dimension ) {
   size_t n = pts.size();
   using vtx = vertex<point, maxK>;
   auto vv = parlay::tabulate(
       n, [&]( size_t i ) -> vtx { return vtx( pts[i], i ); } );
   auto v = parlay::tabulate( n, [&]( size_t i ) -> vtx* { return &vv[i]; } );
   vv.clear(), decltype( vv )().swap( vv );
   ANN<maxK>( v, k );

   // int m = n * k;
   // parlay::sequence<int> Pout( m );
   // parlay::parallel_for( 0, n - 1, [&]( size_t i ) {
   //    for( int j = 0; j < k; j++ ) {
   //       Pout[k * i + j] = ( v[i]->ngh[j] )->identifier;
   //    }
   // } );
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
      coord box_size = 10000;

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

   if( argc >= 4 ) {
      int serialTag = std::stoi( argv[3] );
      if( Dim == 2 ) {
         auto pts = parlay::tabulate( N, [&]( size_t i ) -> point2 {
            return point2( wp[i].pnt[0], wp[i].pnt[1] );
         } );
         wp.clear(), points().swap( wp );
         testZdtree<100, point2>( pts, K, Dim ); //! only valid for k=100
      } else if( Dim == 3 ) {
         auto pts = parlay::tabulate( N, [&]( size_t i ) -> point3 {
            return point3( wp[i].pnt[0], wp[i].pnt[1], wp[i].pnt[2] );
         } );
         wp.clear(), points().swap( wp );
         testZdtree<100, point3>( pts, K, Dim ); //! only valid for k=100
      } else {
         throw( "bad dimension in Zd tree" );
         abort();
      }
   }
}