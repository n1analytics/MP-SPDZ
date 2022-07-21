/*
 * RealProgramParty.cpp
 *
 */

#include "RealProgramParty.h"

#include "Register_inline.h"

#include "Tools/NetworkOptions.h"
#include "Math/Setup.h"

#include "RealGarbleWire.hpp"
#include "CommonParty.hpp"
#include "Register.hpp"
#include "ProgramParty.hpp"
#include "GC/Machine.hpp"
#include "GC/Processor.hpp"
#include "GC/Program.hpp"
#include "GC/Secret.hpp"
#include "GC/Thread.hpp"
#include "GC/ThreadMaster.hpp"
#include "Math/Z2k.hpp"
#include "Protocols/fake-stuff.hpp"

template<class T>
RealProgramParty<T>* RealProgramParty<T>::singleton = 0;

template<class T>
RealProgramParty<T>::RealProgramParty(int argc, const char** argv) :
		garble_processor(garble_machine), dummy_proc({}, 0)
{
	assert(singleton == 0);
	singleton = this;

	ez::ezOptionParser opt;
	opt.add(
			T::dishonest_majority ? "2" : "3", // Default.
			0, // Required?
			1, // Number of args expected.
			0, // Delimiter if expecting multiple args.
			"Number of players", // Help description.
			"-N", // Flag token.
			"--nparties" // Flag token.
	);
	opt.add(
			"", // Default.
			0, // Required?
			0, // Number of args expected.
			0, // Delimiter if expecting multiple args.
			"Evaluate only after garbling.", // Help description.
			"-O", // Flag token.
			"--oneshot" // Flag token.
	);
	opt.parse(argc, argv);
	int nparties;
	opt.get("-N")->getInt(nparties);
	this->check(nparties);
	one_shot = opt.isSet("-O");

	NetworkOptions network_opts(opt, argc, argv);
	OnlineOptions& online_opts = OnlineOptions::singleton;
	if (T::needs_ot)
	    online_opts = {opt, argc, argv, 1000};
	else
	    online_opts = {opt, argc, argv};
	assert(not online_opts.interactive);

	online_opts.finalize(opt, argc, argv);
	this->load(online_opts.progname);

	auto& N = this->N;
	auto& P = this->P;
	auto& delta = this->delta;
	auto& mac_key = this->mac_key;
	auto& garble_processor = this->garble_processor;
	auto& prng = this->prng;
	auto& program = this->program;
	auto& MC = this->MC;

	this->_id = online_opts.playerno + 1;
	Server::start_networking(N, online_opts.playerno, nparties,
			network_opts.hostname, network_opts.portnum_base);
	if (T::dishonest_majority)
	    P = new PlainPlayer(N, 0);
	else
	    P = new CryptoPlayer(N, 0);

	delta = prng.get_doubleword();
	delta.set_signal(1);
#ifdef VERBOSE
	cerr << "delta: " << delta << endl;
#endif

	string prep_dir = get_prep_sub_dir<T>(nparties);
	usage = DataPositions(N.num_players());
	if (online_opts.live_prep)
	{
		mac_key.randomize(prng);
		if (T::needs_ot)
			BaseMachine::s().ot_setups.push_back({*P, true});
		prep = new typename T::LivePrep(0, usage);
	}
	else
	{
	    T::read_or_generate_mac_key(prep_dir, *P, mac_key);
		prep = new Sub_Data_Files<T>(N, prep_dir, usage);
	}

	MC = new typename T::MAC_Check(mac_key);

	garble_processor.reset(program);
	this->processor.open_input_file(N.my_num(), 0, online_opts.cmd_private_input_file);
	this->processor.setup_redirection(P->my_num(), 0, online_opts, this->processor.out);

	shared_proc = new SubProcessor<T>(dummy_proc, *MC, *prep, *P);

	auto& inputter = shared_proc->input;
	inputter.reset_all(*P);
	for (int i = 0; i < N.num_players(); i++)
		if (i == N.my_num())
			inputter.add_mine(int128(delta.r));
		else
			inputter.add_other(i);
	inputter.exchange();
	for (int i = 0; i < N.num_players(); i++)
		deltas.push_back(inputter.finalize(i));

	garble_inputter = new Inputter(shared_proc, *P);
	garble_protocol = new typename T::Protocol(*P);
	for (int i = 0; i < SPDZ_OP_N; i++)
		this->spdz_wires[i].push_back({});

	this->timer.reset();
	do
	{
		next = GC::TIME_BREAK;
		garble();
		try
		{
			this->online_timer.start();
			if (one_shot)
				this->start_online_round();
			else
			{
				this->load_garbled_circuit();
				next = this->second_phase(program, this->processor,
						this->machine, this->dynamic_memory);
			}
			this->online_timer.stop();
		}
		catch (needs_cleaning& e)
		{
		}
	}
	while (next != GC::DONE_BREAK);

	MC->Check(*P);
	data_sent = P->total_comm().sent;

	if (online_opts.verbose)
	    P->total_comm().print();

	this->machine.write_memory(this->N.my_num());
}

template<class T>
void RealProgramParty<T>::garble()
{
	auto& P = this->P;
	auto& garble_processor = this->garble_processor;
	auto& program = this->program;
	auto& MC = this->MC;

	do
	{
		garble_jobs.clear();
		garble_inputter->reset_all(*P);
		auto& protocol = *garble_protocol;
		protocol.init(*prep, shared_proc->MC);
		protocol.init_mul();

		next = this->first_phase(program, garble_processor, this->garble_machine);

		garble_inputter->exchange();
		protocol.exchange();

		typename T::Protocol second_protocol(*P);
		second_protocol.init(*prep, shared_proc->MC);
		second_protocol.init_mul();
		for (auto& job : garble_jobs)
			job.middle_round(*this, second_protocol);

		second_protocol.exchange();

		vector<T> wires;
		for (auto& job : garble_jobs)
			job.last_round(*this, *garble_inputter, second_protocol, wires);

		vector<typename T::clear> opened;
		MC->POpen(opened, wires, *P);

		LocalBuffer garbled_circuit;
		for (auto& x : opened)
			garbled_circuit.serialize(x);

		this->garbled_circuits.push_and_clear(garbled_circuit);
		this->input_masks_store.push_and_clear(garble_input_masks);
		this->output_masks_store.push_and_clear(garble_output_masks);
	}
	while (one_shot and next == GC::TIME_BREAK);
}

template<class T>
RealProgramParty<T>::~RealProgramParty()
{
	delete shared_proc;
	delete prep;
	delete garble_inputter;
	delete garble_protocol;
	cout << "Data sent = " << data_sent * 1e-6 << " MB" << endl;
}

template<class T>
void RealProgramParty<T>::receive_keys(Register& reg)
{
	auto& _id = this->_id;
	auto& _N = this->_N;
	reg.init(_N);
	reg.keys[0][_id - 1] = this->prng.get_doubleword();
	reg.keys[0][_id - 1].set_signal(0);
	reg.keys[1][_id - 1] = reg.keys[0][_id - 1] ^ this->get_delta();
}

template<class T>
void RealProgramParty<T>::receive_all_keys(Register& reg, bool external)
{
	(void) reg, (void) external;
	throw not_implemented();
}

template<class T>
void RealProgramParty<T>::process_prf_output(PRFOutputs& prf_output, PRFRegister* out, const PRFRegister* left, const PRFRegister* right)
{
	assert(out != 0 and left != 0 and right != 0);
	auto l = reinterpret_cast<const RealGarbleWire<T>*>(left);
	auto r = reinterpret_cast<const RealGarbleWire<T>*>(right);
	reinterpret_cast<RealGarbleWire<T>*>(out)->garble(prf_output, *l, *r);
}

template<class T>
void RealProgramParty<T>::push_spdz_wire(SpdzOp op, const RealGarbleWire<T>& wire)
{
	DualWire<T> spdz_wire;
	spdz_wire.mask = wire.mask;
	for (int i = 0; i < 2; i++)
		spdz_wire.my_keys[i] = wire.keys[i][this->N.my_num()];
	spdz_wire.pack(this->spdz_wires[op].back());
	this->spdz_storage += sizeof(spdz_wire);
}
