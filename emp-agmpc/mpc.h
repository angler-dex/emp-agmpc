#ifndef CMPC_H__
#define CMPC_H__
#include "fpremp.h"
#include "abitmp.h"
#include "netmp.h"
#include <emp-tool/emp-tool.h>
using namespace emp;

class CMPC { public:
  int nP;
	const static int SSP = 5;//5*8 in fact...
	const block MASK = makeBlock(0x0ULL, 0xFFFFFULL);
	FpreMP* fpre = nullptr;
	block** mac;
	block** key;
	bool* value;

	block** preprocess_mac;
	block** preprocess_key;
	bool* preprocess_value;

	block** sigma_mac;
	block** sigma_key;
	bool * sigma_value;

	block** ANDS_mac;
	block** ANDS_key;
	bool * ANDS_value;

	block * labels;
	bool * mask = nullptr;
	BristolFormat * cf;
	NetIOMP * io;
	int num_ands = 0, num_in;
	int party, total_pre, ssp;
	ThreadPool * pool;
	block Delta;

	block *GTM;
	block *GTK;
	bool *GTv;
	block *GT;
	block **eval_labels;
	PRP prp;

	CMPC(NetIOMP * io[2], ThreadPool * pool, int party, BristolFormat * cf, int ssp = 40)
      : nP(io[0]->nP)
  {
    mac = new block*[nP+1];
    key = new block*[nP+1];
    preprocess_mac = new block*[nP+1];
    preprocess_key = new block*[nP+1];
    sigma_mac = new block*[nP+1];
    sigma_key = new block*[nP+1];
    ANDS_mac = new block*[nP+1];
    ANDS_key = new block*[nP+1];
    eval_labels = new block*[nP+1];

		this->party = party;
		this->io = io[0];
		this->cf = cf;
		this->ssp = ssp;
		this->pool = pool;

		for(int i = 0; i < cf->num_gate; ++i) {
			if (cf->gates[4*i+3] == AND_GATE)
				++num_ands;
		}
		num_in = cf->n1+cf->n2;
		total_pre = num_in + num_ands + 3*ssp;
		fpre = new FpreMP(io, pool, party, ssp);
		Delta = fpre->Delta;

    if (party == 1) {
		  GTM = new block[num_ands*4*(nP+1)];
		  GTK = new block[num_ands*4*(nP+1)];
		  GTv = new bool[num_ands*4];
		  GT = new block[num_ands*(nP+1)*4*(nP+1)];
    }

		labels = new block[cf->num_wire];
		for(int i  = 1; i <= nP; ++i) {
			key[i] = new block[cf->num_wire];
			mac[i] = new block[cf->num_wire];
			ANDS_key[i] = new block[num_ands*3];
			ANDS_mac[i] = new block[num_ands*3];
			preprocess_mac[i] = new block[total_pre];
			preprocess_key[i] = new block[total_pre];
			sigma_mac[i] = new block[num_ands];
			sigma_key[i] = new block[num_ands];
			eval_labels[i] = new block[cf->num_wire];
		}
		value = new bool[cf->num_wire];
		ANDS_value = new bool[num_ands*3];
		preprocess_value = new bool[total_pre + 128]; //+128 because iknp::recv_pre runs off end of array
		sigma_value = new bool[num_ands];
	}
	~CMPC() {
		delete fpre;

    if (party == 1) {
		  delete[] GTM;
		  delete[] GTK;
		  delete[] GTv;
		  delete[] GT;
    }

		delete[] labels;
		for(int i = 1; i <= nP; ++i) {
			delete[] key[i];
			delete[] mac[i];
			delete[] ANDS_key[i];
			delete[] ANDS_mac[i];
			delete[] preprocess_mac[i];
			delete[] preprocess_key[i];
			delete[] sigma_mac[i];
			delete[] sigma_key[i];
			delete[] eval_labels[i];
		}
    delete[] mac;
    delete[] key;
    delete[] preprocess_mac;
    delete[] preprocess_key;
    delete[] sigma_mac;
    delete[] sigma_key;
    delete[] ANDS_mac;
    delete[] ANDS_key;
    delete[] eval_labels;
		delete[] value;
		delete[] ANDS_value;
		delete[] preprocess_value;
		delete[] sigma_value;
	}
	PRG prg;

	void function_independent() {
		if(party != 1)
			prg.random_block(labels, cf->num_wire);

		fpre->compute(ANDS_mac, ANDS_key, ANDS_value, num_ands);

		prg.random_bool(preprocess_value, total_pre+128);//+128 because iknp::recv_pre runs off end of array
		fpre->abit->compute(preprocess_mac, preprocess_key, preprocess_value, total_pre);
		auto ret = fpre->abit->check(preprocess_mac, preprocess_key, preprocess_value, total_pre);
ret.get();

		for(int i = 1; i <= nP; ++i) {
			memcpy(key[i], preprocess_key[i], num_in * sizeof(block));
			memcpy(mac[i], preprocess_mac[i], num_in * sizeof(block));
		}
		memcpy(value, preprocess_value, num_in * sizeof(bool));
#ifdef __debug
		check_MAC(io, ANDS_mac, ANDS_key, ANDS_value, Delta, num_ands*3, party);
		check_correctness(io, ANDS_value, num_ands, party);
#endif
//		ret.get();
	}

	void function_dependent() {
		int ands = num_in;
		//bool * x[nP+1];
		//bool * y[nP+1];
		bool **x = new bool*[nP+1];
		bool **y = new bool*[nP+1];
		for(int i = 1; i <= nP; ++i) {
			x[i] = new bool[num_ands];
			y[i] = new bool[num_ands];
		}

		for(int i = 0; i < cf->num_gate; ++i) {
			if (cf->gates[4*i+3] == AND_GATE) {
				for(int j = 1; j <= nP; ++j) {
					key[j][cf->gates[4*i+2]] = preprocess_key[j][ands];
					mac[j][cf->gates[4*i+2]] = preprocess_mac[j][ands];
				}
				value[cf->gates[4*i+2]] = preprocess_value[ands];
				++ands;
			}
		}

		for(int i = 0; i < cf->num_gate; ++i) {
			if (cf->gates[4*i+3] == XOR_GATE) {
				for(int j = 1; j <= nP; ++j) {
					key[j][cf->gates[4*i+2]] = key[j][cf->gates[4*i]] ^ key[j][cf->gates[4*i+1]];
					mac[j][cf->gates[4*i+2]] = mac[j][cf->gates[4*i]] ^ mac[j][cf->gates[4*i+1]];
				}
				value[cf->gates[4*i+2]] = value[cf->gates[4*i]] != value[cf->gates[4*i+1]];
				if(party != 1)
					labels[cf->gates[4*i+2]] = labels[cf->gates[4*i]] ^ labels[cf->gates[4*i+1]];
			} else if (cf->gates[4*i+3] == NOT_GATE) {
				for(int j = 1; j <= nP; ++j) {
					key[j][cf->gates[4*i+2]] = key[j][cf->gates[4*i]];
					mac[j][cf->gates[4*i+2]] = mac[j][cf->gates[4*i]];
				}
				value[cf->gates[4*i+2]] = value[cf->gates[4*i]];
				if(party != 1)
					labels[cf->gates[4*i+2]] = labels[cf->gates[4*i]] ^ Delta;
			}
		}

#ifdef __debug
		check_MAC(io, mac, key, value, Delta, cf->num_wire, party);
#endif

		ands = 0;
		for(int i = 0; i < cf->num_gate; ++i) {
			if (cf->gates[4*i+3] == AND_GATE) {
				x[party][ands] = value[cf->gates[4*i]] != ANDS_value[3*ands];
				y[party][ands] = value[cf->gates[4*i+1]] != ANDS_value[3*ands+1];	
				ands++;
			}
		}

		vector<future<void>>	 res;
		for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if( (i < j) and (i == party or j == party) ) {
			int party2 = i + j - party;
			res.push_back(pool->enqueue([this, x, y, party2]() {
				io->send_data(party2, x[party], num_ands);
				io->send_data(party2, y[party], num_ands);
				io->flush(party2);
			}));
			res.push_back(pool->enqueue([this, x, y, party2]() {
				io->recv_data(party2, x[party2], num_ands);
				io->recv_data(party2, y[party2], num_ands);
			}));
		}
		joinNclean(res);
		for(int i = 2; i <= nP; ++i) for(int j = 0; j < num_ands; ++j) {
			x[1][j] = x[1][j] != x[i][j];
			y[1][j] = y[1][j] != y[i][j];
		}

		ands = 0;
		for(int i = 0; i < cf->num_gate; ++i) {
			if (cf->gates[4*i+3] == AND_GATE) {
				for(int j = 1; j <= nP; ++j) {
					sigma_mac[j][ands] = ANDS_mac[j][3*ands+2];
					sigma_key[j][ands] = ANDS_key[j][3*ands+2];
				}
				sigma_value[ands] = ANDS_value[3*ands+2];

				if(x[1][ands]) {
					for(int j = 1; j <= nP; ++j) {
						sigma_mac[j][ands] = sigma_mac[j][ands] ^ ANDS_mac[j][3*ands+1];
						sigma_key[j][ands] = sigma_key[j][ands] ^ ANDS_key[j][3*ands+1];
					}
					sigma_value[ands] = sigma_value[ands] != ANDS_value[3*ands+1];
				}
				if(y[1][ands]) {
					for(int j = 1; j <= nP; ++j) {
						sigma_mac[j][ands] = sigma_mac[j][ands] ^ ANDS_mac[j][3*ands];
						sigma_key[j][ands] = sigma_key[j][ands] ^ ANDS_key[j][3*ands];
					}
					sigma_value[ands] = sigma_value[ands] != ANDS_value[3*ands];
				}
				if(x[1][ands] and y[1][ands]) {
					if(party != 1)
						sigma_key[1][ands] = sigma_key[1][ands] ^ Delta;
					else
						sigma_value[ands] = not sigma_value[ands];
				}
				ands++;
			}
		}//sigma_[] stores the and of input wires to each AND gates
#ifdef __debug_
		check_MAC(io, sigma_mac, sigma_key, sigma_value, Delta, num_ands, party);
		ands = 0;
		for(int i = 0; i < cf->num_gate; ++i) {
			if (cf->gates[4*i+3] == AND_GATE) {
				bool tmp[] = { value[cf->gates[4*i]], value[cf->gates[4*i+1]], sigma_value[ands]};
				check_correctness(io, tmp, 1, party);
				ands++;
			}
		}
#endif

		ands = 0;
		block *H[4];
    for (int i=0; i<4; i++) {
      H[i] = new block[nP+1];
    }
		block K[4][nP+1], M[4][nP+1];
		bool r[4];
		if(party != 1) { 
			for(int i = 0; i < cf->num_gate; ++i) if(cf->gates[4*i+3] == AND_GATE) {
				r[0] = sigma_value[ands] != value[cf->gates[4*i+2]];
				r[1] = r[0] != value[cf->gates[4*i]];
				r[2] = r[0] != value[cf->gates[4*i+1]];
				r[3] = r[1] != value[cf->gates[4*i+1]];

				for(int j = 1; j <= nP; ++j) {
					M[0][j] = sigma_mac[j][ands] ^ mac[j][cf->gates[4*i+2]];
					M[1][j] = M[0][j] ^ mac[j][cf->gates[4*i]];
					M[2][j] = M[0][j] ^ mac[j][cf->gates[4*i+1]];
					M[3][j] = M[1][j] ^ mac[j][cf->gates[4*i+1]];

					K[0][j] = sigma_key[j][ands] ^ key[j][cf->gates[4*i+2]];
					K[1][j] = K[0][j] ^ key[j][cf->gates[4*i]];
					K[2][j] = K[0][j] ^ key[j][cf->gates[4*i+1]];
					K[3][j] = K[1][j] ^ key[j][cf->gates[4*i+1]];
				}
				K[3][1] = K[3][1] ^ Delta;

				Hash(H, labels[cf->gates[4*i]], labels[cf->gates[4*i+1]], ands);
				for(int j = 0; j < 4; ++j) {
					for(int k = 1; k <= nP; ++k) if(k != party) {
						H[j][k] = H[j][k] ^ M[j][k];
						H[j][party] = H[j][party] ^ K[j][k];
					}
					H[j][party] = H[j][party] ^ labels[cf->gates[4*i+2]];
					if(r[j]) 
						H[j][party] = H[j][party] ^ Delta;
				}
				for(int j = 0; j < 4; ++j) {
					//io->send_data(1, H[j]+1, sizeof(block)*(nP));
					io->send_data(1, &H[j][1], sizeof(block)*(nP));
        }
				++ands;
			}
			io->flush(1);
		} else {
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, party2]() {
					for(int i = 0; i < num_ands; ++i) {
						for(int j = 0; j < 4; ++j) {
							io->recv_data(party2, &mat4di(GT, i,party2,j,1, num_ands,nP+1,4,nP+1),
                            sizeof(block)*(nP));
            }
          }
				}));
			}
			for(int i = 0; i < cf->num_gate; ++i) if(cf->gates[4*i+3] == AND_GATE) {
				r[0] = sigma_value[ands] != value[cf->gates[4*i+2]];
				r[1] = r[0] != value[cf->gates[4*i]];
				r[2] = r[0] != value[cf->gates[4*i+1]];
				r[3] = r[1] != value[cf->gates[4*i+1]];
				r[3] = r[3] != true;

				for(int j = 1; j <= nP; ++j) {
					M[0][j] = sigma_mac[j][ands] ^ mac[j][cf->gates[4*i+2]];
					M[1][j] = M[0][j] ^ mac[j][cf->gates[4*i]];
					M[2][j] = M[0][j] ^ mac[j][cf->gates[4*i+1]];
					M[3][j] = M[1][j] ^ mac[j][cf->gates[4*i+1]];

					K[0][j] = sigma_key[j][ands] ^ key[j][cf->gates[4*i+2]];
					K[1][j] = K[0][j] ^ key[j][cf->gates[4*i]];
					K[2][j] = K[0][j] ^ key[j][cf->gates[4*i+1]];
					K[3][j] = K[1][j] ^ key[j][cf->gates[4*i+1]];
				}
				memcpy(&mat3di(GTK, ands,0,0, num_ands,4,nP+1), K, sizeof(block)*4*(nP+1));
				memcpy(&mat3di(GTM, ands,0,0, num_ands,4,nP+1), M, sizeof(block)*4*(nP+1));
				memcpy(&mat2di(GTv, ands,0, num_ands,4), r, sizeof(bool)*4);
				++ands;
			}
			joinNclean(res);
		}
    for (int i=0; i<4; i++) {
      delete[] H[i];
    }
		for(int i = 1; i <= nP; ++i) {
			delete[] x[i];
			delete[] y[i];
		}
    delete[] x;
    delete[] y;
	}

	void online (bool * input, bool * output) {
		bool * mask_input = new bool[cf->num_wire];
		for(int i = 0; i < num_in; ++i)
			mask_input[i] = input[i] != value[i];
		if(party != 1) {
			io->send_data(1, mask_input, num_in);
			io->flush(1);
			io->recv_data(1, mask_input, num_in);
		} else {
			//bool * tmp[nP+1];
			bool **tmp = new bool*[nP+1];
			for(int i = 2; i <= nP; ++i) tmp[i] = new bool[num_in];
			vector<future<void>> res;
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, tmp, party2]() {
					io->recv_data(party2, tmp[party2], num_in);
				}));
			}
			joinNclean(res);
			for(int i = 0; i < num_in; ++i)
				for(int j = 2; j <= nP; ++j)
					mask_input[i] = tmp[j][i] != mask_input[i];
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, mask_input, party2]() {
					io->send_data(party2, mask_input, num_in);
					io->flush(party2);
				}));
			}
			joinNclean(res);
			for(int i = 2; i <= nP; ++i) delete[] tmp[i];
      delete[] tmp;
		}
	
		if(party!= 1) {
			for(int i = 0; i < num_in; ++i) {
				block tmp = labels[i];
				if(mask_input[i]) tmp = tmp ^ Delta;
				io->send_data(1, &tmp, sizeof(block));
			}
			io->flush(1);
		} else {
			vector<future<void>> res;
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, party2]() {
					io->recv_data(party2, eval_labels[party2], num_in*sizeof(block));
				}));
			}
			joinNclean(res);
	
			int ands = 0;	
			for(int i = 0; i < cf->num_gate; ++i) {
				if (cf->gates[4*i+3] == XOR_GATE) {
					for(int j = 2; j<= nP; ++j)
						eval_labels[j][cf->gates[4*i+2]] = eval_labels[j][cf->gates[4*i]] ^ eval_labels[j][cf->gates[4*i+1]];
					mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i]] != mask_input[cf->gates[4*i+1]];
				} else if (cf->gates[4*i+3] == AND_GATE) {
					int index = 2*mask_input[cf->gates[4*i]] + mask_input[cf->gates[4*i+1]];
					block H[nP+1];
					for(int j = 2; j <= nP; ++j) {
						//eval_labels[j][cf->gates[4*i+2]] = GTM[ands][index][j];
						eval_labels[j][cf->gates[4*i+2]] = mat3di(GTM, ands,index,j, num_ands,4,nP+1);
          }
					//mask_input[cf->gates[4*i+2]] = GTv[ands][index];
					mask_input[cf->gates[4*i+2]] = mat2di(GTv, ands,index, num_ands,4);
					for(int j = 2; j <= nP; ++j) {
						Hash(H, eval_labels[j][cf->gates[4*i]], eval_labels[j][cf->gates[4*i+1]], ands, index);
						//xorBlocks_arr(H, H, GT[ands][j][index], nP+1);
						xorBlocks_arr(H, H, &mat4di(GT, ands,j,index,0, num_ands,nP+1,4,nP+1), nP+1);
						for(int k = 2; k <= nP; ++k)
							eval_labels[k][cf->gates[4*i+2]] = H[k] ^ eval_labels[k][cf->gates[4*i+2]];
					
						//block t0 = GTK[ands][index][j] ^ Delta;
						block t0 = mat3di(GTK, ands,index,j, num_ands,4,nP+1) ^ Delta;

						//if(cmpBlock(&H[1], &GTK[ands][index][j], 1))
						if(cmpBlock(&H[1], &mat3di(GTK, ands,index,j, num_ands,4,nP+1), 1))
							mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i+2]] != false;
						else if(cmpBlock(&H[1], &t0, 1))
							mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i+2]] != true;
						else 	{cout <<ands <<"no match GT!"<<endl<<flush;
						}
					}
					ands++;
				} else {
					mask_input[cf->gates[4*i+2]] = not mask_input[cf->gates[4*i]];	
					for(int j = 2; j <= nP; ++j)
						eval_labels[j][cf->gates[4*i+2]] = eval_labels[j][cf->gates[4*i]];
				}
			}
		}
		if(party != 1) {
			io->send_data(1, value+cf->num_wire - cf->n3, cf->n3);
			io->flush(1);
		} else {
			vector<future<void>> res;
			//bool * tmp[nP+1];
			bool **tmp = new bool*[nP+1];
			for(int i = 2; i <= nP; ++i) 
				tmp[i] = new bool[cf->n3];
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, tmp, party2]() {
					io->recv_data(party2, tmp[party2], cf->n3);
				}));
			}
			joinNclean(res);
			for(int i = 0; i < cf->n3; ++i)
				for(int j = 2; j <= nP; ++j)
					mask_input[cf->num_wire - cf->n3 + i] = tmp[j][i] != mask_input[cf->num_wire - cf->n3 + i];
			for(int i = 0; i < cf->n3; ++i)
					mask_input[cf->num_wire - cf->n3 + i] = value[cf->num_wire - cf->n3 + i] != mask_input[cf->num_wire - cf->n3 + i];

			for(int i = 2; i <= nP; ++i) delete[] tmp[i];
      delete[] tmp;
			memcpy(output, mask_input + cf->num_wire - cf->n3, cf->n3);
		}
		delete[] mask_input;
	}
	void Hash(block **H, const block & a, const block & b, uint64_t idx) {
		block T[4];
		T[0] = sigma(a);
		T[1] = sigma(a ^ Delta);
		T[2] = sigma(sigma(b));
		T[3] = sigma(sigma(b ^ Delta));
		
		H[0][0] = T[0] ^ T[2];  
		H[1][0] = T[0] ^ T[3];  
		H[2][0] = T[1] ^ T[2];  
		H[3][0] = T[1] ^ T[3];  
		for(int j = 0; j < 4; ++j) for(int i = 1; i <= nP; ++i) {
			H[j][i] = H[j][0] ^ makeBlock(4*idx+j, i);
		}
		for(int j = 0; j < 4; ++j) {
			prp.permute_block(&H[j][1], nP);
		}
	}

	void Hash(block *H, const block &a, const block& b, uint64_t idx, uint64_t row) {
		H[0] = sigma(a) ^ sigma(sigma(b));
		for(int i = 1; i <= nP; ++i) {
			H[i] = H[0] ^ makeBlock(4*idx+row, i);
		}
		prp.permute_block(&H[1], nP);
	}

	string tostring(bool a) {
		if(a) return "T";
		else return "F";
	}

	bool *mask_input;
	//char *outputVals[nP+1];
	char **outputVals;

	void online (bool * input, bool * output, int* start, int* end, bool broadcast_output = false) {
		mask_input = new bool[cf->num_wire];
		//bool * input_mask[nP+1];
		bool **input_mask = new bool*[nP+1];
		for(int i = 0; i <= nP; ++i) input_mask[i] = new bool[end[party] - start[party]];
		memcpy(input_mask[party], value+start[party], end[party] - start[party]);
		memcpy(input_mask[0], input+start[party], end[party] - start[party]);

		vector<future<bool>> res;
		for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
			int party2 = i + j - party;
			res.push_back(pool->enqueue([this, start, end, party2]() {
				char dig[Hash::DIGEST_SIZE];
				io->send_data(party2, value+start[party2], end[party2]-start[party2]);
				emp::Hash::hash_once(dig, mac[party2]+start[party2], (end[party2]-start[party2])*sizeof(block));
				io->send_data(party2, dig, Hash::DIGEST_SIZE);
				io->flush(party2);
				return false;
			}));
			res.push_back(pool->enqueue([this, start, end, input_mask, party2]() {
				char dig[Hash::DIGEST_SIZE];
				char dig2[Hash::DIGEST_SIZE];
				io->recv_data(party2, input_mask[party2], end[party]-start[party]);
				block * tmp = new block[end[party]-start[party]];
				for(int i =  0; i < end[party] - start[party]; ++i) {
					tmp[i] = key[party2][i+start[party]];
					if(input_mask[party2][i])tmp[i] = tmp[i] ^ Delta;
				}
				emp::Hash::hash_once(dig2, tmp, (end[party]-start[party])*sizeof(block));
				io->recv_data(party2, dig, Hash::DIGEST_SIZE);
				delete[] tmp;
				return strncmp(dig, dig2, Hash::DIGEST_SIZE) != 0;	
			}));
		}
		if(joinNcleanCheat(res)) error("cheat!");
		for(int i = 1; i <= nP; ++i)
			for(int j = 0; j < end[party] - start[party]; ++j)
				input_mask[0][j] = input_mask[0][j] != input_mask[i][j];


		if(party != 1) {
			io->send_data(1, input_mask[0], end[party] - start[party]);
			io->flush(1);
			io->recv_data(1, mask_input, num_in);
		} else {
			vector<future<void>> res;
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, start, end , party2]() {
					io->recv_data(party2, mask_input+start[party2], end[party2] - start[party2]);
				}));
			}
			joinNclean(res);
			memcpy(mask_input, input_mask[0], end[1]-start[1]);
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, party2]() {
					io->send_data(party2, mask_input, num_in);
					io->flush(party2);
				}));
			}
			joinNclean(res);
		}

		for(int i = 0; i <= nP; ++i)
      delete[] input_mask[i];
    delete[] input_mask;
	
		if(party!= 1) {
			for(int i = 0; i < num_in; ++i) {
				block tmp = labels[i];
				if(mask_input[i]) tmp = tmp ^ Delta;
				io->send_data(1, &tmp, sizeof(block));
			}
			io->flush(1);
		} else {
			vector<future<void>> res;
			for(int i = 2; i <= nP; ++i) {
				int party2 = i;
				res.push_back(pool->enqueue([this, party2]() {
					io->recv_data(party2, eval_labels[party2], num_in*sizeof(block));
				}));
			}
			joinNclean(res);
	
			int ands = 0;	
			for(int i = 0; i < cf->num_gate; ++i) {
				if (cf->gates[4*i+3] == XOR_GATE) {
					for(int j = 2; j<= nP; ++j)
						eval_labels[j][cf->gates[4*i+2]] = eval_labels[j][cf->gates[4*i]] ^ eval_labels[j][cf->gates[4*i+1]];
					mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i]] != mask_input[cf->gates[4*i+1]];
				} else if (cf->gates[4*i+3] == AND_GATE) {
					int index = 2*mask_input[cf->gates[4*i]] + mask_input[cf->gates[4*i+1]];
					block H[nP+1];
					for(int j = 2; j <= nP; ++j) {
						//eval_labels[j][cf->gates[4*i+2]] = GTM[ands][index][j];
						eval_labels[j][cf->gates[4*i+2]] = mat3di(GTM, ands,index,j, num_ands,4,nP+1);
          }
					//mask_input[cf->gates[4*i+2]] = GTv[ands][index];
					mask_input[cf->gates[4*i+2]] = mat2di(GTv, ands,index, num_ands,4);
					for(int j = 2; j <= nP; ++j) {
						Hash(H, eval_labels[j][cf->gates[4*i]], eval_labels[j][cf->gates[4*i+1]], ands, index);
						//xorBlocks_arr(H, H, GT[ands][j][index], nP+1);
						xorBlocks_arr(H, H, &mat4di(GT, ands,j,index,0, num_ands,nP+1,4,nP+1), nP+1);
						for(int k = 2; k <= nP; ++k)
							eval_labels[k][cf->gates[4*i+2]] = H[k] ^ eval_labels[k][cf->gates[4*i+2]];
					
						//block t0 = GTK[ands][index][j] ^ Delta;
						block t0 = mat3di(GTK, ands,index,j, num_ands,4,nP+1) ^ Delta;

						//if(cmpBlock(&H[1], &GTK[ands][index][j], 1))
						if(cmpBlock(&H[1], &mat3di(GTK, ands,index,j, num_ands,4,nP+1), 1))
							mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i+2]] != false;
						else if(cmpBlock(&H[1], &t0, 1))
							mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i+2]] != true;
						else 	{cout <<ands <<"no match GT!"<<endl<<flush;
						}
					}
					ands++;
				} else {
					mask_input[cf->gates[4*i+2]] = not mask_input[cf->gates[4*i]];	
					for(int j = 2; j <= nP; ++j)
						eval_labels[j][cf->gates[4*i+2]] = eval_labels[j][cf->gates[4*i]];
				}
			}
		}

		if(broadcast_output) {
            // party 1 sends masked circuit evaluation result
		    //if(party != 1) {
			//    io->recv_data(1, mask_input + cf->num_wire - cf->n3, cf->n3);
		    //} else {
		    //	for(int i = 2; i <= nP; ++i)
		    //	    io->send_data(i, mask_input + cf->num_wire - cf->n3, cf->n3);
            //}

            // everyone broadcasts output (no mac checking).
            // must run before winner is selected to prevent malicious abort
        outputVals = new char*[nP+1];
        for(int i = 0; i <= nP; ++i)
		      outputVals[i] = new char[cf->n3];

		    for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
		    	int party2 = i + j - party;
		    	res.push_back(pool->enqueue([this, party2]() -> bool {
		    		io->send_data(party2, value + cf->num_wire - cf->n3, cf->n3);
                    io->flush(party2);
		    		return false;
		    	}));
		    	res.push_back(pool->enqueue([this, party2]() -> bool {
		    		io->recv_data(party2, outputVals[party2], cf->n3);
                    return false;
		    	}));
            }
		    if(joinNcleanCheat(res)) error("output collection\n");

            // only party 1 can reconstruct
            if (party == 1) {
                memcpy(output, mask_input + cf->num_wire - cf->n3, cf->n3);

		        for(int i = 0; i < cf->n3; ++i) {
		        	for(int j = 1; j <= nP; ++j) {
                        if (j == party)
		        		    output[i] = value[cf->num_wire - cf->n3 + i] != output[i];
                        else
		        		    output[i] = outputVals[j][i] != output[i];
                    }
                }
            }


		    //block *tmp_macs[nP+1];
		    //block *tmp_labels[nP+1];
		    ////bool *tmp_mask_inputs[nP+1];
            //for(int i = 0; i <= nP; ++i) {
		    //    tmp_macs[i] = new block[cf->n3];
		    //    tmp_labels[i] = new block[cf->n3];
		    //    tmp_mask_inputs[i] = new bool[cf->n3];
            //}
            //std::mutex output_m;

		    //for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
		    //	int party2 = i + j - party;

		    //	res.push_back(pool->enqueue([this, mask_input, party2]() -> bool {
		    //	    io->send_data(party2, mac[party2] + cf->num_wire - cf->n3, cf->n3);
		    //	    io->send_data(party2, labels + cf->num_wire - cf->n3, cf->n3);
		    //	    io->send_data(party2, mask_input + cf->num_wire - cf->n3, cf->n3);
		    //		//io->send_data(party2, value + cf->num_wire - cf->n3, cf->n3);
		    //		return false;
		    //	}));

		    //	res.push_back(pool->enqueue([this, output, &output_m, tmp_macs, tmp_labels, tmp_mask_inputs, party2]() -> bool {
		    //	    io->recv_data(party2, tmp_macs[party2] + cf->num_wire - cf->n3, cf->n3);
		    //	    io->recv_data(party2, tmp_labels[party2] + cf->num_wire - cf->n3, cf->n3);
		    //	    //io->recv_data(party2, mask_input + cf->num_wire - cf->n3, cf->n3);
		    //		io->recv_data(party2, tmp_mask_inputs[party2], cf->n3);

            //        for(int g = 0; g < cf->n3; ++g) {
			//		    block tmp = tmp_macs[party2][g];
			//		    tmp =  tmp & MASK;

			//		    block ttt = key[party2][cf->num_wire - cf-> n3 + g] ^ fpre->Delta;
			//		    ttt =  ttt & MASK;
			//		    key[party2][cf->num_wire - cf-> n3 + g] = key[party2][cf->num_wire - cf-> n3 + g] & MASK;

			//		    if(cmpBlock(&tmp, &key[party2][cf->num_wire - cf-> n3 + g], 1)) {
			//		    	output[g] = false;
            //            } else if(cmpBlock(&tmp, &ttt, 1)) {
			//		    	output[g] = true;
			//		    } else {
            //                cout <<"no match output label! party2: "<<party2<<" gate: "<<g<<endl;
            //                cout <<"tmp: " << tmp << endl;
            //                cout <<"key: " << key[party2][cf->num_wire - cf-> n3 + g] << endl;
            //                cout <<"ttt: " << ttt << endl;
            //                return true;
            //            }
			//		    block mask_label = tmp_labels[party2][g];
			//		    if(tmp_mask_inputs[party2][g])
			//		    	mask_label = mask_label ^ fpre->Delta;
			//		    mask_label = mask_label & MASK;
			//		    block masked_labels = labels[cf->num_wire - cf-> n3 + g] & MASK;
			//		    if(!cmpBlock(&mask_label, &masked_labels, 1)) {
            //                cout <<"no match output label2! party2: "<<party2<<" gate: "<<g<<endl;
            //                cout <<"mask_label: " << mask_label << endl;
            //                cout <<"masked_labels: " << masked_labels << endl;
            //                return true;
            //            }

            //            {
            //                std::lock_guard<std::mutex> lg(output_m);
			//		        output[g] = output[g] != tmp_mask_inputs[party2][g];
			//		        output[g] = output[g] != getLSB(mac[party2][cf->num_wire - cf->n3 + g]);
            //            }
            //        }
            //        return false;
		    //	}));
            //}
		    //if(joinNcleanCheat(res)) error("output collection\n");

            //for(int i = 0; i <= nP; ++i) {
            //    delete[] tmp_macs[i];
            //    delete[] tmp_labels[i];
            //    delete[] tmp_mask_inputs[i];
            //}

		} else { // only alice gets output
		    if(party != 1) {
		    	io->send_data(1, value+cf->num_wire - cf->n3, cf->n3);
		    	io->flush(1);
		    } else {
		    	vector<future<void>> res;
		    	//bool * tmp[nP+1];
		    	bool **tmp = new bool*[nP+1];
		    	for(int i = 2; i <= nP; ++i)
		    		tmp[i] = new bool[cf->n3];
		    	for(int i = 2; i <= nP; ++i) {
		    		int party2 = i;
		    		res.push_back(pool->enqueue([this, tmp, party2]() {
		    			io->recv_data(party2, tmp[party2], cf->n3);
		    		}));
		    	}
		    	joinNclean(res);
		    	for(int i = 0; i < cf->n3; ++i)
		    		for(int j = 2; j <= nP; ++j)
		    			mask_input[cf->num_wire - cf->n3 + i] = tmp[j][i] != mask_input[cf->num_wire - cf->n3 + i];
		    	for(int i = 0; i < cf->n3; ++i)
		    			mask_input[cf->num_wire - cf->n3 + i] = value[cf->num_wire - cf->n3 + i] != mask_input[cf->num_wire - cf->n3 + i];

		    	for(int i = 2; i <= nP; ++i) delete[] tmp[i];
          delete[] tmp;
		    	memcpy(output, mask_input + cf->num_wire - cf->n3, cf->n3);
		    }
		    delete[] mask_input;
        }
	}

    // output is ignored if party==1
    // openToParty is ignored if party!=1
    bool broadcast_output(bool* output, int openToParty=-1) {
        bool ret = false;

		if(party != 1) {
            // have we been selected to receive output
            bool selected = false;
		    io->recv_data(1, &selected, sizeof(bool));

            if (selected) {
                // receive masked circuit eval result
		        io->recv_data(1, mask_input + cf->num_wire - cf->n3, cf->n3);
                // check mac - TODO

                // reconstruct output
		        for(int i = 0; i < cf->n3; ++i) {
		        	for(int j = 1; j <= nP; ++j) {
                        if (j == party)
		        		    mask_input[cf->num_wire - cf->n3 + i] = value[cf->num_wire - cf->n3 + i] != mask_input[cf->num_wire - cf->n3 + i];
                        else
		        		    mask_input[cf->num_wire - cf->n3 + i] = outputVals[j][i] != mask_input[cf->num_wire - cf->n3 + i];
                    }
                }
		        memcpy(output, mask_input + cf->num_wire - cf->n3, cf->n3);
                ret = true;
            }
        } else {
		    vector<future<void>> res;

            // tell relevant party(ies) to expect output
			for(int i = 2; i <= nP; ++i) {
		    	res.push_back(pool->enqueue([this, i, openToParty]() {
                    bool selected = openToParty == -1 || openToParty == i;
		            io->send_data(i, &selected, sizeof(bool));
				    io->flush(i);
                }));
            }

            // sends masked circuit evaluation result
			for(int i = 2; i <= nP; ++i) {
                bool selected = openToParty == -1 || openToParty == i;
                if (selected) {
		    	    res.push_back(pool->enqueue([this, i]() {
			            io->send_data(i, mask_input + cf->num_wire - cf->n3, cf->n3);
                        // send macs - TODO
				        io->flush(i);
                    }));
                }
            }
		    joinNclean(res);
        }
		delete[] mask_input;
		for(int i = 0; i <= nP; ++i) {
			delete[] outputVals[i];
        }
		delete[] outputVals;
        return ret;
    }
};
#endif// CMPC_H__
