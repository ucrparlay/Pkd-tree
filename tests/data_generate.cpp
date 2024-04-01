#include <string>
#include "kdTree.h"
#include <filesystem>

const double EPS = 1e-9;
#define rep(i, a, b) for (int i = (a); i < (b); i++)
#define per(i, a, b) for (int i = (a); i > (b); i--)
#define LL           long long
#define Lson         (index * 2)
#define Rson         (index * 2 + 1)
#define MOD          ((int)1000000007)
#define MAXN         1000 + 5
///**********************************START*********************************///
long long N = 8e5;
long long Dim = 5;
long long numFile = 3;

using Typename = coord;
const Typename dataRange = 1e6;

// std::string path = "../benchmark/craft_var_node_integer";
std::string path = "/data3/zmen002/kdtree/uniform";

inline std::string toString(const long long& a) {
  return std::to_string(a);
}

Point<Typename>* wp;

void generatePoints(std::ofstream& f) {
  wp = new Point<Typename>[N];
  coord box_size = 1e6;

  std::random_device rd;      // a seed source for the random number engine
  std::mt19937 gen_mt(rd());  // mersenne_twister_engine seeded with rd()
  // std::uniform_real_distribution<Typename> distrib( 1, box_size );
  std::uniform_int_distribution<int> distrib(1, box_size);

  parlay::random_generator gen(distrib(gen_mt));
  // std::uniform_real_distribution<Typename> dis( -box_size, box_size );
  // std::uniform_int_distribution<int> dis( -box_size, box_size );
  std::uniform_int_distribution<int> dis(0, box_size);

  // generate n random points in a cube
  parlay::parallel_for(
      0, N,
      [&](long i) {
        auto r = gen[i];
        for (int j = 0; j < Dim; j++) {
          wp[i].x[j] = dis(r);
        }
      },
      1000);

  f << N << " " << Dim << std::endl;
  for (size_t i = 0; i < N; i++) {
    for (int j = 0; j < Dim; j++) {
      f << wp[i].x[j] << " ";
    }
    f << std::endl << std::flush;
  }

  delete[] wp;
}

// std::string path = "../benchmark/craft_var_node_integer";
std::default_random_engine generator;
struct kd_node_t {
  Typename x[15];
  struct kd_node_t *left, *right;
  int num;  // number of nodes in subtree plus itself
};

kd_node_t* node;

inline double getRealRandom(const double& a, const double& b) {
  std::uniform_real_distribution<double> distribution(a, b);
  return distribution(generator);
}

inline int getIntRandom(const int& a, const int& b) {
  std::uniform_int_distribution<int> distribution(a, b);
  return distribution(generator);
}

void generatePointsSerial(std::ofstream& f) {
  node = (kd_node_t*)malloc(N * sizeof(kd_node_t));
  f << N << " " << Dim << std::endl;
  for (int i = 0; i < N; i++) {
    int idx = 0;
    Typename a;
    for (int j = 0; j < Dim; j++) {
      if constexpr (std::is_integral_v<Typename>)
        // WARN: serial will generate negative numbers
        a = getIntRandom(-dataRange, dataRange);
      else if (std::is_floating_point_v<Typename>) {
        a = getRealRandom(-dataRange, dataRange);
      }
      node[i].x[j] = a;
      f << a << " ";
    }
    f << std::endl;
  }
  free(node);
}

int main(int argc, char* argv[]) {
  assert(argc >= 4);
  path = std::string(argv[1]);
  N = std::stoll(argv[2]);
  Dim = std::stoll(argv[3]);
  numFile = std::stoll(argv[4]);
  int serial = std::stoi(argv[5]);

  path += "/" + toString(N) + "_" + toString(Dim) + "/";
  std::filesystem::create_directory(path);
  std::ofstream f;

  for (long long i = 0; i < numFile; i++) {
    std::string newpath = path + toString(i + 1) + ".in";
    std::cout << newpath << std::endl;
    f.open(newpath);
    if (serial)
      generatePointsSerial(f);
    else
      generatePoints(f);
    f.close();
  }
  return 0;
}
