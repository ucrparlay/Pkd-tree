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
   parlay::internal::timer timer;

   std::string name( argv[1] );
   name = name.substr( name.rfind( "/" ) + 1 );
   std::cout << name << " ";

   timer.start();
   setup( argv[1] );
   timer.stop();
   std::cout << timer.total_time() << " ";

   timer.start();
   query_k_Nearest();
   timer.stop();
   std::cout << timer.total_time() << std::endl;

   return 0;
}