#include <emp-tool/emp-tool.h>
#include "emp-agmpc/emp-agmpc.h"
using namespace std;
using namespace emp;

int main(int argc, char** argv) {
  int party, port;
  const static int nP = kIpPorts.size();
	parse_party_and_port(argv, &party, &port);
	if(party > nP)return 0;
	ThreadPool pool(2*(nP-1)+2);
	NetIOMP io(party, &pool);
	NetIOMP io2(party, &pool);
	NetIOMP *ios[2] = {&io, &io2};

	FpreMP mp(ios, &pool, party);

	int num_ands = 1<<17;
	block * mac[nP+1];
	block * key[nP+1];
	bool * value;

	for(int i = 1; i <= nP; ++i) {
		key[i] = new block[num_ands*3];
		mac[i] = new block[num_ands*3];
	}
	value = new bool[num_ands*3];
	auto t1 = clock_start();
	mp.compute(mac, key, value, num_ands);
	cout <<"Gates: "<<num_ands<<" time: "<<time_from(t1)<<endl;
	return 0;
}
