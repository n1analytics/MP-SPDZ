/*
 * Party.cpp
 *
 */

#include "Party.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "Tools/callgrind.h"

#include "proto_utils.h"
#include "msg_types.h"
#include "prf.h"
#include "BooleanCircuit.h"
#include "Math/Setup.h"

#include "Register_inline.h"

#include "CommonParty.hpp"
#include "ProgramParty.hpp"
#include "Protocols/MAC_Check.hpp"
#include "BMR/Register.hpp"
#include "GC/Machine.hpp"
#include "GC/Processor.hpp"
#include "GC/Secret.hpp"
#include "GC/Thread.hpp"
#include "GC/ThreadMaster.hpp"
#include "GC/Program.hpp"
#include "Processor/Instruction.hpp"
#include "Protocols/Share.hpp"

ProgramParty* ProgramParty::singleton = 0;


BaseParty::BaseParty()
{
#ifdef DEBUG_PRNG_PARTY
	octet seed[SEED_SIZE];
	memset(seed, 0, sizeof(seed));
	seed[0] = id;
	prng.SetSeed(seed);
#endif
}

BaseParty::~BaseParty()
{
}

void BaseParty::NodeReady()
{
	printf("Node is ready\n");
}

void BaseParty::NewMessage(int from, ReceivedMsg& msg)
{
	(void) from;
	char* message = msg.data();
	size_t len = msg.size();
//	printf("got message of len %u from %d\n", len, from);
	MSG_TYPE message_type;
	msg.unserialize(message_type);
#ifdef DEBUG_STEPS
	cout << "processing " << message_type_names[message_type] << " from " << dec
			<< from << " of length " << msg.size() << endl;
#endif
	unique_lock<mutex> locker(global_lock);
	switch(message_type) {
	case TYPE_KEYS:
		{
#ifdef DEBUG_STEPS
		printf("TYPE_KEYS\n");
#endif
#ifdef DEBUG2
		cout << "received keys" << endl;
		phex(message, len);
#endif
		get_buffer(TYPE_PRF_OUTPUTS);
		_compute_prfs_outputs((Key*)(message + MSG_KEYS_HEADER_SZ));
		_send_prfs();
//		printf("sent prfs\n");
		break;
		}
	case TYPE_MASK_INPUTS:
		{
#ifdef DEBUG_STEPS
		printf("TYPE_MASK_INPUTS\n");
#endif
#ifdef DEBUG_INPUT
		cout << "received " << msg.left() << " input masks" << endl;
#endif
		mask_input(msg);
		break;
		}
	case TYPE_MASK_OUTPUT:
		{
#ifdef DEBUG_STEPS
		printf("TYPE_MASK_OUTPUT\n");
#endif
#ifdef DEBUG_OUTPUT_MASKS
		cout << "receiving " << msg.left() << " output masks" << endl;
#endif
		mask_output(msg);
		break;
		}
	case TYPE_GARBLED_CIRCUIT:
	{
#ifdef DEBUG_STEPS
		printf("TYPE_GARBLED_CIRCUIT\n");
#endif
#ifdef DEBUG2
		phex(message, len);
#endif
#ifdef DEBUG_COMM
		cout << "got " << len << " bytes for " << get_garbled_tbl_size() << " gates" << endl;
#endif
		if ((len - 4) != 4 * _N * sizeof(Key) * get_garbled_tbl_size())
			throw runtime_error("wrong size of garbled table");
		store_garbled_circuit(msg);

//				printf("\nGarbled Table\n\n");
//				_printf_garbled_table();
//		char garbled_circuit_cs = cs((char*)_garbled_tbl , garbled_tbl_sz);
//		printf ("\ngarbled_circuit_cs = %d\n", garbled_circuit_cs);

		_node->Send(SERVER_ID, get_buffer(TYPE_RECEIVED_GC));

		break;
	}
	case TYPE_LAUNCH_ONLINE:
	{
#ifdef DEBUG_STEPS
		printf("TYPE_LAUNCH_ONLINE\n");
#endif
		cout << "Launching online phase at " << timer.elapsed() << endl;
		_node->print_waiting();
		// store a token item in case it's needed just before ending the program
		ReceivedMsg msg;
		// to avoid copying from address 0
		msg.resize(1);
		// for valgrind
		msg.data()[0] = 0;
		// token input round
		store_garbled_circuit(msg);
		online_timer.start();
		start_online_round();
		break;
	}
	case TYPE_SPDZ_WIRES:
		receive_spdz_wires(msg);
		break;
	case TYPE_DELTA:
		msg.unserialize(delta);
		break;
	default:
		{
		printf("UNDEFINED\n");
		printf("got undefined message\n");
		phex(message, len);
		break;
		}
	}

#ifdef DEBUG_STEPS
	cout << "done with " << message_type_names[message_type] << " from " << from << endl;
#endif
}

void BaseParty::Start()
{
	_node->Start();
}


void BaseParty::_send_prfs() {
	_node->Send(SERVER_ID, buffers[TYPE_PRF_OUTPUTS]);
#ifdef DEBUG2
	printf("Send PRFs:\n");
	phex(buffers[TYPE_PRF_OUTPUTS]);
#endif
}




void BaseParty::done() {
	cout << "Online phase took " << online_timer.elapsed() << " seconds" << endl;
	_node->Send(SERVER_ID, get_buffer(TYPE_DONE));
	_node->Stop();
}

ProgramParty::ProgramParty() :
        		spdz_storage(0), garbled_storage(0), spdz_counters(SPDZ_OP_N),
        		processor(machine),
        		prf_processor(prf_machine),
				P(0)
{
	if (singleton)
		throw runtime_error("there can only be one");
	singleton = this;
	threshold = 128;
	eval_threads = new Worker<AndJob>[N_EVAL_THREADS];
	and_jobs.resize(N_EVAL_THREADS);
}

FakeProgramParty::FakeProgramParty(int argc, const char** argv) :
		keys_for_prf(0)
{
	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <id> <program> [netmap]" << endl;
		exit(1);
	}

	load(argv[2]);
	_id = atoi(argv[1]);
	processor.open_input_file("user_inputs/user_" + to_string(_id - 1) + "_input.txt");

	if (argc > 3)
	{
		int n_parties = init(argv[3], _id);
		ifstream netmap(argv[3]);
		int tmp;
		string tmp2;
		netmap >> tmp >> tmp2 >> tmp;
		vector<string> hostnames(n_parties);
		for (int i = 0; i < n_parties; i++)
		{
			netmap >> hostnames[i];
			netmap >> tmp;
		}
		N.init(_id - 1, 5000, hostnames);
	}
	else
	{
		int n_parties = init("LOOPBACK", _id);
		N.init(_id - 1, 5000, vector<string>(n_parties, "localhost"));
	}
	ifstream schfile((string("Programs/Schedules/") + argv[2] + ".sch").c_str());
	string curr, prev;
	while (schfile.good())
	{
		prev = curr;
		getline(schfile, curr);
	}
	cout << "Compiler: " << prev << endl;
	P = new PlainPlayer(N, 0);
	if (argc > 4)
		threshold = atoi(argv[4]);
	cout << "Threshold for multi-threaded evaluation: " << threshold << endl;
}
//
ProgramParty::~ProgramParty()
{
	reset();
	if (P)
	{
		cerr << "Data sent: " << 1e-6 * P->total_comm().total_data() << " MB" << endl;
		delete P;
	}
	delete[] eval_threads;
#ifdef VERBOSE
	if (spdz_counters[SPDZ_LOAD])
	    cerr << "SPDZ loading: " << spdz_counters[SPDZ_LOAD] << endl;
	if (spdz_counters[SPDZ_STORE])
	    cerr << "SPDZ storing: " << spdz_counters[SPDZ_STORE] << endl;
	if (spdz_storage)
	    cerr << "SPDZ wire storage: " << 1e-9 * spdz_storage << " GB" << endl;
	cerr << "Maximum circuit storage: " << 1e-9 * garbled_storage << " GB" << endl;
#endif
}

FakeProgramParty::~FakeProgramParty()
{
#ifdef VERBOSE
    if (dynamic_memory.capacity_in_bytes())
        cerr << "Dynamic storage: " << 1e-9 * dynamic_memory.capacity_in_bytes()
                << " GB" << endl;
#endif
}

void FakeProgramParty::_compute_prfs_outputs(Key* keys)
{
	keys_for_prf = keys;
	first_phase(program, prf_processor, prf_machine);
}

void FakeProgramParty::_check_evaluate()
{
	FakeProgramPartySuper::_check_evaluate();
}

void ProgramParty::reset()
{
	CommonParty::reset();
}

void ProgramParty::store_garbled_circuit(ReceivedMsg& msg)
{
	garbled_storage = max(msg.size(), garbled_storage);
	garbled_circuits.push(msg);
}

void ProgramParty::load_garbled_circuit()
{
	if (not garbled_circuits.pop(garbled_circuit))
		throw runtime_error("no garbled circuit available");
	if (not output_masks_store.pop(output_masks))
		throw runtime_error("no output masks available");
	if (not input_masks_store.pop(input_masks))
		throw runtime_error("no input masks available");
#ifdef DEBUG_OUTPUT_MASKS
	cout << "loaded " << output_masks.left() << " output masks" << endl;
#endif
#ifdef DEBUG_INPUT
    cout << "loaded " << input_masks.left() << " input masks" << endl;
#endif
}

void ProgramParty::start_online_round()
{
	machine.reset_timer();
	_check_evaluate();
}

void FakeProgramParty::receive_keys(Register& reg)
{
	reg.init(_N);
	for (int i = 0; i < 2; i++)
		reg.keys[i][_id-1] = *(keys_for_prf++);
#ifdef DEBUG
	cout << "receive keys " << reg.get_id() << "(" << &reg << ") " << dec << reg.keys[0].size() << endl;
	reg.keys.print(reg.get_id(), _id);
	cout << "delta " << reg.get_id() << " " << (reg.keys[0][_id-1] ^ reg.keys[1][_id-1]) << endl;
#endif
}

void FakeProgramParty::receive_all_keys(Register& reg, bool external)
{
	reg.init(get_n_parties());
	for (int i = 0; i < get_n_parties(); i++)
		reg.keys[external][i] = *(keys_for_prf++);
}

void FakeProgramParty::process_prf_output(PRFOutputs& prf_output,
		PRFRegister* wire, const PRFRegister* left, const PRFRegister* right)
{
	(void) wire, (void) left, (void) right;
	prf_output.serialize(buffers[TYPE_PRF_OUTPUTS], _id, get_n_parties());
}

void FakeProgramParty::receive_spdz_wires(ReceivedMsg& msg)
{
	int op;
	msg.unserialize(op);
	spdz_wires[op].push_back({});
	size_t l = msg.left();
	spdz_wires[op].back().append((octet*)msg.consume(l), l);
	spdz_storage += l;
#ifdef DEBUG_SPDZ_WIRES
	cout << "receive " << dec << spdz_wires[op].back().get_length() << "/"
			<< msg.size() << " bytes for type " << op << endl;
#endif
	if (op == SPDZ_MAC)
	{
		gf2n_long spdz_mac_key;
		spdz_mac_key.unpack(spdz_wires[op].back());
		if (!MC)
		{
			MC = new MAC_Check_<Share<gf2n_long>>(spdz_mac_key);
			cout << "MAC key: " << hex << spdz_mac_key << endl;
			mac_key = spdz_mac_key;
		}
	}
}

void ProgramParty::store_wire(const Register& reg)
{
	wires.serialize(reg.key(get_id(), 0));
#ifdef DEBUG
	cout << "storing wire" << endl;
	reg.print();
#endif
}

void ProgramParty::load_wire(Register& reg)
{
	wires.unserialize(reg.key(get_id(), 0));
	reg.key(get_id(), 1) = reg.key(get_id(), 0) ^ get_delta();
#ifdef DEBUG
	cout << "loading wire" << endl;
	reg.print();
#endif
}
