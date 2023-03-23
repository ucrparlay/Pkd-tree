#include <parlay/delayed_sequence.h>
#include <parlay/internal/get_time.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

#include <iostream>

#include "kdTree.h"

int
main( int argc, char* argv[] )
{

   // int a[] = { 10, 6, 4, 5, 2, 8, 7, 9, 3, 1, 2, 3, 9 };
   // int k = 1;
   // kArrayQueue Q;
   // Q.init( k );
   // for( int i = 0; i < 13; i++ )
   // {
   //    Q.insert( a[i] );
   //    std::cout << Q.getLoad() << std::endl;
   //    Q.printQueue();
   // }
   // std::cout << Q.queryKthElement() << std::endl;

   parlay::internal::timer timer;

   std::string name( argv[1] );
   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   timer.start();
   if( argc >= 2 )
   {
      setup( argv[1], "16" );
   }
   else if( argc >= 3 )
   {
      setup( argv[1], argv[2] );
   }
   timer.stop();
   std::cout << timer.total_time() << " ";

   timer.start();
   query_k_Nearest();
   timer.stop();
   std::cout << timer.total_time() << " ";

   std::cout << ( argc >= 3 ? argv[2] : "16" ) << std::endl;
   return 0;
}