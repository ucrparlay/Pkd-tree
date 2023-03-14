#pragma GCC optimize( 3 )

#include <bits/stdc++.h>
const size_t MAX_DIM = 15;
const size_t MAXN = 2e6 + 5;
const long double eps = 1e-10;

int N, Dim, Q, K;
std::priority_queue<long double> q;

struct kd_node_t
{
   long double x[MAX_DIM];
   struct kd_node_t *left, *right;
   int num;                  // number of nodes in subtree plus itself
   long double mxx[MAX_DIM]; // mxx[i] the maximum of sub points on dimension i
   long double mnx[MAX_DIM]; // mnx[i] the minimum
};

struct node_cmp
{
   node_cmp( size_t index ) : index_( index ) {}
   bool
   operator()( const kd_node_t& n1, const kd_node_t& n2 ) const
   {
      return n1.x[index_] < n2.x[index_];
   }
   size_t index_;
};

inline bool
Gt( const long double& a, const long double& b )
{
   return a - b > eps;
}

inline bool
Lt( const long double& a, const long double& b )
{
   return a - b < -eps;
}

inline bool
Eq( const long double& a, const long double& b )
{
   return std::abs( a - b ) < eps;
}

inline bool
Geq( const long double& a, const long double& b )
{
   return Gt( a, b ) || Eq( a, b );
}

inline bool
Leq( const long double& a, const long double& b )
{
   return Lt( a, b ) || Eq( a, b );
}

struct kd_node_t *root, *found, wp[MAXN];

inline long double
dist( struct kd_node_t* a, struct kd_node_t* b, int dim )
{
   long double t, d = 0.0;
   while( dim-- )
   {
      t = a->x[dim] - b->x[dim];
      d += t * t;
   }
   return d;
}

inline void
swap_node( struct kd_node_t* x, struct kd_node_t* y )
{
   long double tmp[MAX_DIM];
   memcpy( tmp, x->x, sizeof( tmp ) );
   memcpy( x->x, y->x, sizeof( tmp ) );
   memcpy( y->x, tmp, sizeof( tmp ) );
}

struct kd_node_t*
find_median( struct kd_node_t* start, struct kd_node_t* end, int idx )
{
   if( end <= start )
      return NULL;
   if( end == start + 1 )
      return start;

   struct kd_node_t *p, *store, *md = start + ( ( end - start ) / 2 ),
                                *bk = start;
   long double pivot;

   int l = 0, r = 0, mid = ( end - start ) >> 1;

   while( 1 )
   {
      pivot = md->x[idx];

      swap_node( md, end - 1 );
      for( store = p = start; p < end - 1; p++ )
      {
         if( p->x[idx] - pivot < -eps )
         {
            if( p != store )
               swap_node( p, store );
            store++;
         }
      }
      swap_node( store, end - 1 );

      if( store->x[idx] == md->x[idx] )
         return md;
      if( abs( store - bk ) > abs( md - bk ) )
         end = store;
      else
         start = store;
   }
}

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

struct kd_node_t*
make_tree( struct kd_node_t* a, int len, int i, int dim )
{
   struct kd_node_t* root;
   if( !len )
      return 0;

   // std::sort( a, a + len, node_cmp( i ) );

   std::nth_element( a, a + len / 2, a + len, node_cmp( i ) );
   i = ( i + 1 ) % dim;
   root = a + ( len >> 1 );
   root->left = make_tree( a, root - a, i, dim );
   root->right = make_tree( root + 1, a + len - ( root + 1 ), i, dim );

   //* begin augment
   for( int i = 0; i < dim; i++ )
      root->mnx[i] = root->mxx[i] = root->x[i];
   root->num = 1;

   if( root->left )
      augment( root, root->left, dim );
   if( root->right )
      augment( root, root->right, dim );

   // std::cout << root->x[0] << " " << root->num << std::endl;
   // for( int i = 0; i < dim; i++ )
   //    std::cout << root->mxx[i] << " " << root->mnx[i] << std::endl;
   // puts( "---" );
   return root;
}

void
k_nearest( struct kd_node_t* root, struct kd_node_t* nd, int i, int dim )
{
   long double d, dx, dx2;

   if( !root )
      return;
   d = dist( root, nd, dim );
   dx = root->x[i] - nd->x[i];
   dx2 = dx * dx;
   // printf( "%Lf %Lf\n", sqrt( d ), q.size() ? sqrt( q.top() ) : -1 );
   //* q.top() return the largest element
   if( q.size() < K || d - q.top() < eps )
   {
      q.push( d );
      if( q.size() > K )
         q.pop();
   }

   if( ++i >= dim )
      i = 0;

   k_nearest( dx > 0 ? root->left : root->right, nd, i, dim );
   if( dx2 - q.top() >= eps && q.size() >= K )
      return;
   k_nearest( dx > 0 ? root->right : root->left, nd, i, dim );
}

// nd: node used to query
void
nearest( struct kd_node_t* root, struct kd_node_t* nd, long double& nDist,
         int i, int dim )
{
   long double d, dx, dx2;

   if( !root )
      return;
   d = dist( root, nd, dim );
   dx = root->x[i] - nd->x[i];
   dx2 = dx * dx;

   if( d < nDist )
      nDist = d;
   if( ++i >= dim )
      i = 0;
   nearest( dx > 0 ? root->left : root->right, nd, nDist, i, dim );
   if( dx2 - nDist >= eps )
      return;
   nearest( dx > 0 ? root->right : root->left, nd, nDist, i, dim );
}

void
query_k_Nearest()
{
   int i, j;
   scanf( "%d", &Q );
   struct kd_node_t z;
   while( Q-- )
   {
      scanf( "%d", &K );
      for( j = 0; j < Dim; j++ )
         scanf( "%Lf", &z.x[j] );
      while( !q.empty() )
         q.pop();
      k_nearest( root, &z, 0, Dim );
      // printf( "%ld\n", q.size() );
      // printf( "%.8Lf\n", sqrtl( q.top() ) );
   }
}

void
queryNearest()
{
   int i, j;
   scanf( "%d", &Q );
   struct kd_node_t z;
   while( Q-- )
   {
      long double nDist = LDBL_MAX;
      scanf( "%d", &K );
      for( j = 0; j < Dim; j++ )
         scanf( "%Lf", &z.x[j] );
      nearest( root, &z, nDist, 0, Dim );
      // printf( "%ld\n", q.size() );
      printf( "%.8Lf\n", sqrtl( nDist ) );

      //* check
      for( int i = 0; i < N; i++ )
      {
         if( dist( wp + i, &z, Dim ) - nDist < -eps )
         {
            printf( "wrong\n" );
         }
      }
   }
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
         scanf( "%Lf %Lf", &zl.x[j], &zr.x[j] );
         if( Gt( zl.x[j], zr.x[j] ) )
            std::swap( zl.x[j], zr.x[j] );
      }
      rangeQuery( root, &zl, &zr, cnt, 0, Dim );
      printf( "%d\n", cnt );

      //* check
   }
}

void
setup( char* path )
{
   freopen( path, "r", stdin );

   scanf( "%d %d", &N, &Dim );
   int i, j;

   for( i = 0; i < N; i++ )
   {
      for( j = 0; j < Dim; j++ )
      {
         scanf( "%Lf", &wp[i].x[j] );
      }
   }

   root = make_tree( wp, N, 0, Dim );
   // queryNearest();
}
