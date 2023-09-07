#pragma once

#include "testFramework.h"

constexpr int MAX_DIM = 15;

template<typename T>
class Point {
 public:
  void
  print() {
    for ( int i = 0; i < 3; i++ ) {
      std::cout << x[i] << " ";
    }
  };
  void
  print( int d ) {
    for ( int i = 0; i < d; i++ ) {
      std::cout << x[i] << " ";
    }
  }

 public:
  int id;
  T x[MAX_DIM];
};

template<typename T>
struct PointCompare {
 public:
  PointCompare( size_t index ) : index_( index ) {}
  bool
  operator()( const Point<T>& n1, const Point<T>& n2 ) const {
    return n1.x[index_] < n2.x[index_];
  }
  size_t index_;
};

template<typename T>
inline T
dist( Point<T>* a, Point<T>* b, int dim ) {
  T t, d = 0;
  while ( dim-- ) {
    t = a->x[dim] - b->x[dim];
    d += t * t;
  }
  return d;
}

template<typename T>
class KDnode {
 public:
  void
  print() {
    std::cout << isLeaf << " " << num << std::endl;
    for ( int i = 0; i < num; i++ ) {
      p[i].print();
      puts( "" );
    }
  }

 public:
  KDnode(){};
  Point<T>* p;
  KDnode<T>*left = nullptr, *right = nullptr;
  bool isLeaf = false;
  int num = 1;
};

template<typename T>
class KDtree {
 public:
  KDtree() {}

  KDnode<T>*
  init( const int& _DIM, const int& _LEAVE_WRAP, Point<T>* a, int len );

  KDnode<T>*
  make_tree( Point<T>* a, int len, int i );

  void
  k_nearest( KDnode<T>* root, Point<T>* nd, int i, kBoundedQueue<T>& q,
             size_t& visNodeNum );

  void
  k_nearest_array( KDnode<T>* root, Point<T>* nd, int i, kArrayQueue<T>& kq );

  T
  query_k_nearest( Point<T>* nd, int _K ) {
    this->K = _K;
    parlay::sequence<coord> array_queue( K );
    kBoundedQueue<T> bq( array_queue.cut( 0, K ) );
    size_t visNodeNUm = 0;
    this->k_nearest( this->KDroot, nd, 0, bq, visNodeNUm );
    T ans = bq.top();
    return ans;
  };

  T
  query_k_nearest_array( Point<T>* nd, int _K ) {
    this->K = _K;
    kArrayQueue<T> kq;
    kq.resize( _K );
    this->k_nearest_array( this->KDroot, nd, 0, kq );
    T ans = kq.queryKthElement();
    return ans;
  };

  void
  destory( KDnode<T>* root );

 public:
  int DIM;
  int K;
  int LEAVE_WRAP = 16;
  KDnode<T>* KDroot;

  // ArrayQueue<T> aq;
  // CGAL::internal::bounded_priority_queue<T> bq;
};