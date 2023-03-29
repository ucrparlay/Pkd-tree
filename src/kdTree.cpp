#include "kdTree.h"

template <typename T>
void
KDtree<T>::init( const int& _DIM, const int& _LEAVE_WRAP, Point<T>* a, int len )
{
   this->DIM = _DIM;
   this->LEAVE_WRAP = _LEAVE_WRAP;
   this->KDroot = this->make_tree( a, len, 0 );
   this->kq.init( 300 );
   return;
}

template <typename T>
KDnode<T>*
KDtree<T>::make_tree( Point<T>* a, int len, int i )
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

   std::nth_element( a, a + len / 2, a + len, PointCompare<T>( i ) );
   i = ( i + 1 ) % this->DIM;

   Point<T>* median = a + ( len >> 1 );
   root->p = new Point<T>[1];
   root->p[0] = *median;

   root->left = make_tree( a, median - a, i );
   root->right = make_tree( median + 1, a + len - ( median + 1 ), i );

   return root;
}

template <typename T>
void
KDtree<T>::k_nearest( KDnode<T>* root, Point<T>* nd, int i )
{
   double d, dx, dx2;
   // root->print();

   if( root == nullptr )
      return;
   if( root->isLeaf )
   {
      for( int i = 0; i < root->num; i++ )
      {
         d = dist( &( root->p[i] ), nd, DIM );
         if( q.size() < K || Lt( d, q.top() ) )
         {
            q.push( d );
            if( q.size() > K )
               q.pop();
         }
      }
      return;
   }

   d = dist( &( root->p[0] ), nd, DIM );
   dx = ( root->p[0] ).x[i] - nd->x[i];
   dx2 = dx * dx;
   //* q.top() return the largest element
   if( q.size() < K || Lt( d, q.top() ) )
   {
      q.push( d );
      if( q.size() > K )
         q.pop();
   }

   if( ++i >= DIM )
      i = 0;

   k_nearest( dx > 0 ? root->left : root->right, nd, i );
   if( Gt( dx2, q.top() ) && q.size() >= K )
      return;
   k_nearest( dx > 0 ? root->right : root->left, nd, i );
}

template <typename T>
void
KDtree<T>::k_nearest_array( KDnode<T>* root, Point<T>* nd, int i )
{
   double d, dx, dx2;
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
   //* q.top() return the largest element
   kq.insert( d );

   if( ++i >= DIM )
      i = 0;

   k_nearest_array( dx > 0 ? root->left : root->right, nd, i );
   if( Gt( dx2, kq.queryKthElement() ) && kq.getLoad() >= this->K )
      return;
   k_nearest_array( dx > 0 ? root->right : root->left, nd, i );
}

template class Point<double>;
template class PointCompare<double>;
template class KDnode<double>;
template class KDtree<double>;