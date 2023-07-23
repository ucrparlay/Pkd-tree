#pragma GCC optimize( 3 )
#include "kdTree.h"
#include "utility.h"
const double EPS = 1e-9;
#define rep( i, a, b ) for( int i = ( a ); i < ( b ); i++ )
#define per( i, a, b ) for( int i = ( a ); i > ( b ); i-- )
#define LL long long
#define Lson ( index * 2 )
#define Rson ( index * 2 + 1 )
#define MOD ( (int)1000000007 )
#define MAXN 1000 + 5
///**********************************START*********************************///
long long N = 8e5;
long long Dim = 5;
long long numFile = 3;

// std::string path = "../benchmark/craft_var_node_integer";
std::string path = "/ssd0/zmen002/kdtree/uniform";

inline std::string
toString( const long long& a ) {
   return std::to_string( a );
}

Point<int>* wp;

void
generatePoints( std::ofstream& f ) {
   wp = new Point<int>[N];
   coord box_size = 100000;

   std::random_device rd;       // a seed source for the random number engine
   std::mt19937 gen_mt( rd() ); // mersenne_twister_engine seeded with rd()
   std::uniform_int_distribution<int> distrib( 1, box_size );

   parlay::random_generator gen( distrib( gen_mt ) );
   std::uniform_int_distribution<int> dis( 0, box_size );

   // generate n random points in a cube
   parlay::parallel_for(
       0, N,
       [&]( long i ) {
          auto r = gen[i];
          for( int j = 0; j < Dim; j++ ) {
             wp[i].x[j] = dis( r );
          }
       },
       1000 );

   f << N << " " << Dim << std::endl;
   for( size_t i = 0; i < N; i++ ) {
      for( int j = 0; j < Dim; j++ ) {
         f << wp[i].x[j] << " ";
      }
      f << std::endl;
   }
}

int
main( int argc, char* argv[] ) {
   assert( argc >= 4 );
   N = std::stoll( argv[1] );
   Dim = std::stoll( argv[2] );
   numFile = std::stoll( argv[3] );

   path += "/" + toString( N ) + "_" + toString( Dim ) + "/";
   std::filesystem::create_directory( path );
   std::ofstream f;

   for( long long i = 0; i < numFile; i++ ) {
      std::string newpath = path + toString( i + 1 ) + ".in";
      std::cout << newpath << std::endl;
      f.open( newpath );
      generatePoints( f );
      f.close();
   }
   return 0;
}