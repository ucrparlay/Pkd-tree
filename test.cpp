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
   timer.start();
   if( argc >= 2 )
   {
      setup( argv[1] );
   }
   query_k_Nearest();
   timer.stop();

   std::string resPath( argv[1] );
   resPath = resPath.substr( 0, resPath.rfind( "/" ) ) + "/res.txt";
   std::ofstream f;
   f.open( resPath, std::ios_base::app );
   f << timer.total_time() << std::endl;

   return 0;
}