#ifndef NETIOMP_H__
#define NETIOMP_H__
#include <emp-tool/emp-tool.h>
#include "cmpc_config.h"
#include <stdio.h>
#include <fstream>
using namespace emp;

template<int nP>
class NetIOMP { public:
	NetIO*ios[nP+1];
	NetIO*ios2[nP+1];
	int party;
	bool sent[nP+1];
	NetIOMP(int party, int port) {
		this->party = party;
		memset(sent, false, nP+1);
		for(int i = 1; i <= nP; ++i)for(int j = 1; j <= nP; ++j)if(i < j){
			if(i == party) {
#ifdef LOCALHOST
				ios[j] = new NetIO(IP[j], port+2*(i*nP+j), true);
#else
				ios[j] = new NetIO(IP[j], port+2*(i), true);
#endif

#ifdef LOCALHOST
				ios2[j] = new NetIO(nullptr, port+2*(i*nP+j)+1, true);
#else
				ios2[j] = new NetIO(nullptr, port+2*(j)+1, true);
#endif
			} else if(j == party) {
#ifdef LOCALHOST
				ios[i] = new NetIO(nullptr, port+2*(i*nP+j), true);
#else
				ios[i] = new NetIO(nullptr, port+2*(i), true);
#endif

#ifdef LOCALHOST
				ios2[i] = new NetIO(IP[i], port+2*(i*nP+j)+1, true);
#else
				ios2[i] = new NetIO(IP[i], port+2*(j)+1, true);
#endif
			}
		}
	}
	NetIOMP(int party, std::string ipFilePath, int portOffset, ThreadPool* pool) {
		this->party = party;
		memset(sent, false, nP+1);

        std::ifstream infile(ipFilePath);
        string fileIP[nP+1];
        int filePorts[nP+1];
        int i = 1;
        string ip, port;
        while (getline(infile,ip,':')) {
            getline(infile,port);
            fileIP[i] = ip;
            filePorts[i] = atoi(port.c_str()) + portOffset;
            i++;
            if (i > nP) break;
        }

		vector<future<void>> res;
		for(int i = 1; i <= nP; ++i)for(int j = 1; j <= nP; ++j)if(i < j){
			if(i == party) {
                res.push_back(pool->enqueue([this, fileIP, filePorts, i, j]() {
				    ios[j] = new NetIO(fileIP[j].c_str(), filePorts[j]+2*(i), true);
				    ios2[j] = new NetIO(nullptr, filePorts[i]+2*(j)+1, true);
                }));
			} else if(j == party) {
                res.push_back(pool->enqueue([this, fileIP, filePorts, i, j]() {
				    ios[i] = new NetIO(nullptr, filePorts[j]+2*(i), true);
				    ios2[i] = new NetIO(fileIP[i].c_str(), filePorts[i]+2*(j)+1, true);
                }));
			}
        }
	    for(auto &v: res) v.get();
	    res.clear();
    }

	int64_t count() {
		int64_t res = 0;
#ifdef COUNT_IO
		for(int i = 1; i <= nP; ++i) if(i != party){
			res += ios[i]->counter;
			res += ios2[i]->counter;
		}
#endif
		return res;
	}

	~NetIOMP() {
		for(int i = 1; i <= nP; ++i)
			if(i != party) {
				delete ios[i];
				delete ios2[i];
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
	NetIO*& get(size_t idx, bool b = false){
		if (b)
			return ios[idx];
		else return ios2[idx];
	}
	void flush(int idx = 0) {
		if(idx == 0) {
			for(int i = 1; i <= nP; ++i)
				if(i != party) {
					ios[i]->flush();
					ios2[i]->flush();
				}
		} else {
			if(party < idx)
				ios[idx]->flush();
			else
				ios2[idx]->flush();
		}
	}
	void sync() {
		for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if(i < j) {
			if(i == party) {
				ios[j]->sync();
				ios2[j]->sync();
			} else if(j == party) {
				ios[i]->sync();
				ios2[i]->sync();
			}
		}
	}
};
#endif //NETIOMP_H__
