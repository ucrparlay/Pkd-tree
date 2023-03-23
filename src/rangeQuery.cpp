#include "kdTree.h"

void
augment( kd_node_t* a, kd_node_t* b, int dim )
{
   for( int i = 0; i < dim; i++ )
   {
      a->mxx[i] = Gt( a->mxx[i], b->mxx[i] ) ? a->mxx[i] : b->mxx[i];
      a->mnx[i] = Lt( a->mnx[i], b->mnx[i] ) ? a->mnx[i] : b->mnx[i];
   }
   a->num += b->num;
   return;
}

inline bool
coverSingle( struct kd_node_t* root, struct kd_node_t* L, struct kd_node_t* R,
             int dim )
{
   for( int i = 0; i < dim; i++ )
   {
      if( Lt( root->x[i], L->x[i] ) || Gt( root->x[i], R->x[i] ) )
         return false;
   }
   return true;
}

inline bool
coverMultiple( struct kd_node_t* root, struct kd_node_t* L, struct kd_node_t* R,
               int dim )
{
   for( int i = 0; i < dim; i++ )
   {
      if( Lt( root->mnx[i], L->x[i] ) || Gt( root->mxx[i], R->x[i] ) )
         return false;
   }
   return true;
}

void
rangeQuery( struct kd_node_t* root, struct kd_node_t* L, struct kd_node_t* R,
            int& cnt, int i, int dim )
{
   if( !root )
      return;
   if( coverMultiple( root, L, R, dim ) )
   {
      cnt += root->num;
      return;
   }

   cnt += (int)coverSingle( root, L, R, dim );
   if( ++i >= dim )
      i = 0;

   if( !Gt( L->x[i], root->x[i] ) )
      rangeQuery( root->left, L, R, cnt, i, dim );
   if( !Lt( R->x[i], root->x[i] ) )
      rangeQuery( root->right, L, R, cnt, i, dim );
   return;
}

void
queryRangePoints()
{
   int i, j;
   scanf( "%d", &Q );
   struct kd_node_t zl, zr;
   while( Q-- )
   {
      int cnt = 0;
      scanf( "%d", &K );
      for( j = 0; j < Dim; j++ )
      {
         scanf( "%lf %lf", &zl.x[j], &zr.x[j] );
         if( Gt( zl.x[j], zr.x[j] ) )
            std::swap( zl.x[j], zr.x[j] );
      }
      rangeQuery( root, &zl, &zr, cnt, 0, Dim );
      printf( "%d\n", cnt );

      //* check
   }
}
