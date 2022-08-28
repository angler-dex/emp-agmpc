#include <emp-tool/emp-tool.h>
#include "emp-agmpc/emp-agmpc.h"
#include "test/test.h"
using namespace std;
using namespace emp;

int main(int argc, char** argv) {
  int party, port;
	parse_party_and_port(argv, &party, &port);
  const static int nP = kIpPorts.size();
	if(party > nP)return 0;
	ThreadPool pool(2*(nP-1)+2);
	NetIOMP io(party, &pool);
	NetIOMP io2(party, &pool);
	NetIOMP *ios[2] = {&io, &io2};

	bench_once(party, ios, &pool, circuit_file_location+"AES-non-expanded.txt");
	return 0;
}
