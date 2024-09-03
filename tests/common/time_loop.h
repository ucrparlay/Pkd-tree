#include "parlay/internal/get_time.h"
#include <iostream>
#include <iomanip>

template<class F, class G, class H>
double time_loop(int rounds, double delay, F initf, G runf, H endf) {
    parlay::internal::timer t;
    // run for delay seconds to "warm things up"
    // will skip if delay is zero
    // std::cout << delay << "\n";
    // while (t.total_time() < delay) {
    initf();
    t.start();
    runf();
    std::cout << std::fixed << std::setprecision(4) << t.next_time() << "#" << std::flush;
    endf();
    // }
    t.stop(), t.reset();
    std::cout << "(";
    for (int i = 1; i <= rounds; i++) {
        initf();
        t.start();
        runf();
        std::cout << std::fixed << std::setprecision(4) << t.next_time() << (i == rounds ? ")/3=" : "+") << std::flush;
        if (i == rounds) t.stop();
        endf();
    }
    return t.total_time() / rounds;
}
