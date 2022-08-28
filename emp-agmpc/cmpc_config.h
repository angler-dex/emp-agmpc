#ifndef __CMPC_CONFIG
#define __CMPC_CONFIG
#include <string>
#include <vector>
const static int abit_block_size = 1024;
const static int fpre_threads = 1;

#ifdef __clang__
	#define __MORE_FLUSH
#endif

struct IpPort {
  std::string Ip;
  int port;
};

#define mat2di(mat, i, j, m, n) (mat[((i)*(n)) + (j)])
#define mat3di(mat, i, j, k, m, n, o) (mat[((i)*(n)*(o)) + ((j)*(o)) + (k)])
#define mat4di(mat, i, j, k, l, m, n, o, p) (mat[((i)*(n)*(o)*(p)) + ((j)*(o)*(p)) + ((k)*(p)) + (l)])

const std::vector<IpPort> kIpPorts {
  {"127.0.0.1", 10000},
  {"127.0.0.1", 20000},
  {"127.0.0.1", 30000},
};

const static bool lan_network = true;
#endif// __C2PC_CONFIG
