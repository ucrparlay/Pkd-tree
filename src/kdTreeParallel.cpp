#include <parlay/delayed_sequence.h>
#include <parlay/internal/get_time.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

#include "kdTree.h"
#include <iostream>

int
main()
{
   int n = 10;
   parlay::sequence<double> A( n );
   parlay::parallel_for( 0, n, [&]( size_t i ) { A[i] = i; } );
   for( int i = 0; i < n; i++ )
   {
      std::cout << A[i] << std::endl;
   }
   return 0;
}