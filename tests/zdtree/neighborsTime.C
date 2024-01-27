// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <algorithm>
#include <iostream>

#include "../benchmarks/nearestNeighbors/octTree/neighbors.h"
#include "common/IO.h"
#include "common/parse_command_line.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/random.h"
using namespace benchIO;

// *************************************************************
//  SOME DEFINITIONS
// *************************************************************

using coord = double;
using point2 = point2d<coord>;
using point3 = point3d<coord>;

template <class PT, int KK> struct vertex {
  using pointT = PT;
  int identifier;
  pointT pt;       // the point itself
  vertex *ngh[KK]; // the list of neighbors
  vertex(pointT p, int id) : pt(p), identifier(id) {}
  size_t counter;
  size_t counter2;
};

// *************************************************************
//  TIMING
// *************************************************************

template <int maxK, class point>
void timeNeighbors(parlay::sequence<point> &pts, int k, int rounds,
                   char *outFile, parlay::sequence<point> &pin, int tag,
                   int queryType) {
  size_t n = pts.size();
  using vtx = vertex<point, maxK>;
  int dimensions = pts[0].dimension();
  auto vv =
      parlay::tabulate(n, [&](size_t i) -> vtx { return vtx(pts[i], i); });

  parlay::sequence<point>().swap(pts);

  // auto v = parlay::tabulate( n, [&]( size_t i ) -> vtx* { return &vv[i]; } );

  // std::cout << pts.size() << " " << pin.size() << std::flush;
  parlay::sequence<vtx> pin2;

  if (tag == 2) {
    pin2 =
        parlay::tabulate(pin.size() * batchInsertRatio, [&](size_t i) -> vtx {
          return vtx(pin[i], i + vv.size());
        });
  } else {
    pin2 = parlay::tabulate(pin.size(), [&](size_t i) -> vtx {
      return vtx(pin[i], i + vv.size());
    });
  }

  parlay::sequence<point>().swap(pin);
  // auto vin = parlay::tabulate( pin.size(),
  //                              [&]( size_t i ) -> vtx* { return &pin2[i]; }
  //                              );

  //! cannot remove these two vectors
  //! since it uses pointers for tree
  // decltype( vv )().swap( vv );
  // decltype( pin2 )().swap( pin2 );

  ANN<maxK>(vv, k, rounds, pin2, tag, queryType);

  // if( outFile != NULL ) {
  //   int m = n * k;
  //   parlay::sequence<int> Pout( m );
  //   parlay::parallel_for( 0, n - 1, [&]( size_t i ) {
  //     for( int j = 0; j < k; j++ ) {
  //       Pout[k * i + j] = ( v[i]->ngh[j] )->identifier;
  //     }
  //   } );
  //   writeIntSeqToFile( Pout, outFile );
  // }
}

template <class Point> parlay::sequence<Point> readGeneral(char const *fname) {
  parlay::sequence<char> S = readStringFromFile(fname);
  parlay::sequence<char *> W = stringToWords(S);
  int d = Point::dim;
  //  std::cout << W.size() << std::endl;
  return parsePoints<Point>(W.cut(2, W.size()));
}

int main(int argc, char *argv[]) {
  commandLine P(argc, argv,
                "[-k {1,...,100}] [-d {2,3}] [-o <outFile>] [-r <rounds>] "
                "[-p <inFile>] [-t <tag>] [-q <queryType>] [-i <insertFile>]");
  char *iFile = P.getOptionValue("-p");
  char *oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r", 3);
  int k = P.getOptionIntValue("-k", 100);
  int d = P.getOptionIntValue("-d", 3);
  int tag = P.getOptionIntValue("-t", 0);
  int queryType = P.getOptionIntValue("-q", 0);
  int readInsertFile = P.getOptionIntValue("-i", 1);
  //  algorithm_version = P.getOptionIntValue( "-t", algorithm_version );
  if (k < 1 || k > 100)
    P.badArgument();
  if (d < 2 || d > 3)
    P.badArgument();

  std::string name(iFile);
  name = name.substr(name.rfind("/") + 1);
  std::cout << name << " ";

  if (d == 2) {
    parlay::sequence<point2> PIn = readGeneral<point2>(iFile);
    parlay::sequence<point2> PInsert;
    // parlay::sequence<point2> PIn = readPointsFromFile<point2>( iFile );
    if (readInsertFile == 1) {
      std::string insertFile;
      int id = std::stoi(name.substr(0, name.find_first_of('.')));
      id = (id + 1) % 3; //! MOD graph number used to test
      if (!id)
        id++;
      int pos = std::string(iFile).rfind("/") + 1;
      insertFile =
          std::string(iFile).substr(0, pos) + std::to_string(id) + ".in";
      PInsert = readGeneral<point2>(insertFile.c_str());
    }

    if (k == 1)
      timeNeighbors<1>(PIn, 1, rounds, oFile, PInsert, tag, queryType);
    else
      timeNeighbors<100>(PIn, k, rounds, oFile, PInsert, tag, queryType);
  }

  if (d == 3) {
    parlay::sequence<point3> PIn = readGeneral<point3>(iFile);
    parlay::sequence<point3> PInsert;
    // parlay::sequence<point3> PIn = readPointsFromFile<point3>( iFile );
    if (readInsertFile == 1) {
      std::string insertFile;
      int id = std::stoi(name.substr(0, name.find_first_of('.')));
      id = (id + 1) % 3; //! MOD graph number used to test
      if (!id)
        id++;
      int pos = std::string(iFile).rfind("/") + 1;
      insertFile =
          std::string(iFile).substr(0, pos) + std::to_string(id) + ".in";
      PInsert = readGeneral<point3>(insertFile.c_str());
    }

    if (k == 1)
      timeNeighbors<1>(PIn, 1, rounds, oFile, PInsert, tag, queryType);
    else
      timeNeighbors<100>(PIn, k, rounds, oFile, PInsert, tag, queryType);
  }
}
