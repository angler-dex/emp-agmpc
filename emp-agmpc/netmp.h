#ifndef NETIOMP_H__
#define NETIOMP_H__
#include <emp-tool/emp-tool.h>
#include "cmpc_config.h"
#include <stdio.h>
#include <fstream>
using namespace emp;

class NetIOMP { public:
	int nP;
	int party;
  vector<NetIO*> ios;
  vector<NetIO*> ios2;
  std::vector<bool> sent;

	NetIOMP(int party, ThreadPool* pool)
    : NetIOMP(kIpPorts, party, pool) { }

	NetIOMP(const vector<IpPort> ip, int party, ThreadPool* pool, int port_offset = 0)
      : nP(ip.size()),
        party(party),
        ios(nP+1, nullptr),
        ios2(nP+1, nullptr),
        sent(nP+1, false) {
		vector<future<void>> res;
		for(int i = 1; i <= nP; ++i)for(int j = 1; j <= nP; ++j)if(i < j){
			if(i == party) {
          res.push_back(pool->enqueue([this, &ip, port_offset, i, j]() {
		     	  ios[j] = new NetIO(ip[j-1].Ip.c_str(), ip[j-1].port + (i-1) + port_offset, true);
		     	  ios2[j] = new NetIO(nullptr, ip[i-1].port + (j-1) + port_offset, true);

            //printf("ios[%d] -> %s:%d\n", j, ios[j]->addr.c_str(), ios[j]->port);
            //printf(":%d <- ios2[%d]\n", ios2[j]->port, j);
                }));
			} else if(j == party) {
          res.push_back(pool->enqueue([this, &ip, port_offset, i, j]() {
		    	  ios[i] = new NetIO(nullptr, ip[j-1].port + (i-1) + port_offset, true);
		    	  ios2[i] = new NetIO(ip[i-1].Ip.c_str(), ip[i-1].port + (j-1) + port_offset, true);

            //printf(":%d <- ios[%d]\n", ios[i]->port, i);
            //printf("ios2[%d] -> %s:%d\n", i, ios2[i]->addr.c_str(), ios2[i]->port);
          }));
			}
        }
	  for(auto &v: res) v.get();
	  res.clear();
	}

	int64_t count() {
		int64_t res = 0;
#ifdef COUNT_IO
		for(int i = 1; i <= ios.size(); ++i) {
      if(i != party) {
			  res += ios[i]->counter;
			  res += ios2[i]->counter;
      }
		}
#endif
		return res;
	}

	~NetIOMP() {
		for(int i = 1; i <= nP; ++i) {
			if(i != party) {
				delete ios[i];
				delete ios2[i];
			}
    }
	}
	void send_data(int dst, const void * data, size_t len) {
		if(dst != 0 and dst!= party) {
			if(party < dst)
				ios[dst]->send_data(data, len);
			else
				ios2[dst]->send_data(data, len);
			sent[dst] = true;
		}
#ifdef __MORE_FLUSH
		flush(dst);
#endif
	}
	void recv_data(int src, void * data, size_t len) {
		if(src != 0 and src!= party) {
			//if(sent[src])flush(src); // sent is never set to false!
            //  causes flush on every rx
            //  instead, manually add flushes throughout protocol so it isn't needed.
			if(src < party)
				ios[src]->recv_data(data, len);
			else
				ios2[src]->recv_data(data, len);
		}
	}
	NetIO* get(size_t idx, bool b = false){
		if (b)
			return ios[idx];
		else return ios2[idx];
	}
	void flush(int idx = 0) {
		if(idx == 0) {
			for(int i = 1; i <= nP; ++i) {
				if(i != party) {
					ios[i]->flush();
					ios2[i]->flush();
				}
      }
		} else {
			if(party < idx)
				ios[idx]->flush();
			else
				ios2[idx]->flush();
		}
	}
	void sync() {
		for(int i = 1; i <= nP; ++i) {
      for(int j = 1; j <= nP; ++j) if(i < j) {
			  if(i == party) {
			  	ios[j]->sync();
			  	ios2[j]->sync();
			  } else if(j == party) {
			  	ios[i]->sync();
			  	ios2[i]->sync();
			  }
      }
		}
	}
};
#endif //NETIOMP_H__
