#include "utility.h"

const size_t MAX_DIM = 15;
const size_t MAXN = 2e6 + 5;
size_t LEAVE_WRAP = 16;

int N, Dim, Q, K;
std::priority_queue<double> q;
kArrayQueue kq;

struct kd_node_t
{
   double x[MAX_DIM];
   struct kd_node_t *left, *right;
   bool isLeaf = false;
   int num;             // number of nodes in subtree plus itself
   double mxx[MAX_DIM]; // mxx[i] the maximum of sub points on dimension i
   double mnx[MAX_DIM]; // mnx[i] the minimum
};

struct kd_node_t *root, *wp;

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

inline double
dist( struct kd_node_t* a, struct kd_node_t* b, int dim )
{
   double t, d = 0.0;
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
   double tmp[MAX_DIM];
   memcpy( tmp, x->x, sizeof( tmp ) );
   memcpy( x->x, y->x, sizeof( tmp ) );
   memcpy( y->x, tmp, sizeof( tmp ) );
}

struct kd_node_t*
make_tree( struct kd_node_t* a, int len, int i, int dim )
{
   struct kd_node_t* root;
   if( !len )
      return 0;
   if( len <= LEAVE_WRAP )
   {
      root = a;
      root->isLeaf = true;
      root->num = len;
      return root;
   }

   // std::sort( a, a + len, node_cmp( i ) );

   std::nth_element( a, a + len / 2, a + len, node_cmp( i ) );
   i = ( i + 1 ) % dim;
   root = a + ( len >> 1 );
   root->left = make_tree( a, root - a, i, dim );
   root->right = make_tree( root + 1, a + len - ( root + 1 ), i, dim );

   //* begin augment
   //* for range search
   // for( int i = 0; i < dim; i++ )
   //    root->mnx[i] = root->mxx[i] = root->x[i];
   // root->num = 1;

   // if( root->left )
   //    augment( root, root->left, dim );
   // if( root->right )
   //    augment( root, root->right, dim );

   // std::cout << root->x[0] << " " << root->num << std::endl;
   // for( int i = 0; i < dim; i++ )
   //    std::cout << root->mxx[i] << " " << root->mnx[i] << std::endl;
   // puts( "---" );
   return root;
}

void
k_nearest( struct kd_node_t* root, struct kd_node_t* nd, int i, int dim, int K )
{
   double d, dx, dx2;

   if( !root )
      return;
   if( root->isLeaf )
   {
      for( int i = 0; i < root->num; i++ )
      {
         d = dist( root + i, nd, dim );
         if( q.size() < K || Lt( d, q.top() ) )
         {
            q.push( d );
            if( q.size() > K )
               q.pop();
         }
      }
      return;
   }

   d = dist( root, nd, dim );
   dx = root->x[i] - nd->x[i];
   dx2 = dx * dx;
   //* q.top() return the largest element
   if( q.size() < K || Lt( d, q.top() ) )
   {
      q.push( d );
      if( q.size() > K )
         q.pop();
   }

   if( ++i >= dim )
      i = 0;

   k_nearest( dx > 0 ? root->left : root->right, nd, i, dim, K );
   if( Gt( dx2, q.top() ) && q.size() >= K )
      return;
   k_nearest( dx > 0 ? root->right : root->left, nd, i, dim, K );
}

void
k_nearest_array( struct kd_node_t* root, struct kd_node_t* nd, int i, int dim )
{
   double d, dx, dx2;

   if( !root )
      return;
   if( root->isLeaf )
   {
      for( int i = 0; i < root->num; i++ )
      {
         d = dist( root + i, nd, dim );
         kq.insert( d );
      }
      return;
   }

   d = dist( root, nd, dim ); //* distance from root to query point
   dx = root->x[i] - nd->x[i];
   dx2 = dx * dx; //* distance from root to cutting plane

   kq.insert( d );

   if( ++i >= dim )
      i = 0;

   k_nearest_array( dx > 0 ? root->left : root->right, nd, i, dim );
   if( kq.getLoad() >= K && Gt( dx2, kq.queryKthElement() ) )
      return;
   k_nearest_array( dx > 0 ? root->right : root->left, nd, i, dim );
}

void
clearQueue()
{
   while( !q.empty() )
      q.pop();
}

const double
maxQueue()
{
   return q.top();
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
         scanf( "%lf", &z.x[j] );
      // kq.init( K );
      clearQueue();

      k_nearest( root, &z, 0, Dim, K );
      // k_nearest_array( root, &z, 0, Dim );

      // printf( "%.6lf\n", sqrt( kq.queryKthElement() ) );
      // printf( "%.6lf\n", sqrt( q.top() ) );
   }
}

void
setup( char* path, char* wrap_size )
{
   freopen( path, "r", stdin );

   scanf( "%d %d", &N, &Dim );
   wp = (kd_node_t*)malloc( N * sizeof( kd_node_t ) );
   LEAVE_WRAP = std::stoi( wrap_size );

   for( int i = 0; i < N; i++ )
   {
      for( int j = 0; j < Dim; j++ )
      {
         scanf( "%lf", &wp[i].x[j] );
      }
   }

   root = make_tree( wp, N, 0, Dim );
   // queryNearest();
}
