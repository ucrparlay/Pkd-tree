#include "kdTree.h"

template <typename T>
KDnode<T>*
KDtree<T>::init( const int& _DIM, const int& _LEAVE_WRAP,
                 parlay::sequence<Point<T>> a, int len )
{
   this->DIM = _DIM;
   this->LEAVE_WRAP = _LEAVE_WRAP;
   this->KDroot = this->make_tree( a, len, 0 );
   return this->KDroot;
}

template <typename T>
KDnode<T>*
KDtree<T>::make_tree( parlay::sequence<Point<T>> a, int len, int i )
{
   KDnode<T>* root = new KDnode<T>[1];

   if( !len )
   {
      return nullptr;
   }
   if( len <= this->LEAVE_WRAP )
   {
      root->p = new Point<T>[this->LEAVE_WRAP];

      for( int i = 0; i < len; i++ )
      {
         root->p[i] = a[i];
      }
      root->isLeaf = true;
      root->num = len;
      return root;
   }

   // auto pivot = parlay::kth_smallest( a, len / 2, PointCompare<T>( i ) );

   parlay::sort_inplace( a, PointCompare<T>( i ) );
   i = ( i + 1 ) % this->DIM;

   Point<T>* median = a.begin() + len / 2;
   root->p = new Point<T>[1];
   root->p[0] = *median;

   // assert( *pivot == *median );

   root->left = make_tree( a.subseq( 0, len / 2 ), len / 2, i );
   root->right = make_tree( a.subseq( len / 2, len ), len - len / 2, i );

   return root;
}

template <typename T>
void
KDtree<T>::k_nearest( KDnode<T>* root, Point<T>* nd, int i,
                      kBoundedQueue<T>& q )
{
   T d, dx, dx2;
   // root->print();

   if( root == nullptr )
      return;
   if( root->isLeaf )
   {
      for( int i = 0; i < root->num; i++ )
      {
         d = dist( &( root->p[i] ), nd, DIM );
         q.insert( d );
         // if( q.size() < K || Lt( d, q.top() ) )
         // {
         //    q.push( d );
         //    if( q.size() > K )
         //       q.pop();
         // }
      }

      return;
   }

   d = dist( &( root->p[0] ), nd, DIM );
   dx = ( root->p[0] ).x[i] - nd->x[i];
   dx2 = dx * dx;

   if( ++i >= DIM )
      i = 0;

   k_nearest( dx > 0 ? root->left : root->right, nd, i, q );
   if( Gt( dx2, q.top() ) && q.full() )
      return;
   k_nearest( dx > 0 ? root->right : root->left, nd, i, q );
}

template <typename T>
void
KDtree<T>::k_nearest_array( KDnode<T>* root, Point<T>* nd, int i,
                            kArrayQueue<T>& kq )
{
   T d, dx, dx2;
   // root->print();

   if( root == nullptr )
      return;
   if( root->isLeaf )
   {
      for( int i = 0; i < root->num; i++ )
      {
         d = dist( &( root->p[i] ), nd, DIM );
         kq.insert( d );
      }
      return;
   }

   d = dist( &( root->p[0] ), nd, DIM );
   dx = ( root->p[0] ).x[i] - nd->x[i];
   dx2 = dx * dx;

   // kq.insert( d );

   if( ++i >= DIM )
      i = 0;

   k_nearest_array( dx > 0 ? root->left : root->right, nd, i, kq );
   if( Gt( dx2, kq.queryKthElement() ) && kq.getLoad() >= this->K )
      return;
   k_nearest_array( dx > 0 ? root->right : root->left, nd, i, kq );
}

template <typename T>
void
KDtree<T>::destory( KDnode<T>* root )
{
   if( root->isLeaf )
   {
      delete[] root->p;
      return;
   }
   destory( root->left );
   destory( root->right );
   delete root->left;
   delete root->right;
   return;
}

template class Point<double>;
template class PointCompare<double>;
template class KDnode<double>;
template class KDtree<double>;

template class Point<long>;
template class PointCompare<long>;
template class KDnode<long>;
template class KDtree<long>;

template class Point<long long>;
template class PointCompare<long long>;
template class KDnode<long long>;
template class KDtree<long long>;