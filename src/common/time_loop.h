#include "parlay/internal/get_time.h"

template <class F, class G, class H>
double
time_loop( int rounds, double delay, F initf, G runf, H endf ) {
   parlay::internal::timer t;
   // run for delay seconds to "warm things up"
   // will skip if delay is zero
   while( t.total_time() < delay ) {
      initf();
      runf();
      endf();
   }
   for( int i = 0; i < rounds; i++ ) {
      initf();
      t.start();
      runf();
      t.next_time();
      if( i != rounds - 1 )
         endf();
   }
   return t.total_time() / rounds;
}
