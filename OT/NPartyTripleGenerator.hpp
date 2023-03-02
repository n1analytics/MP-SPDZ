#ifndef OT_NPARTYTRIPLGENERATOR_HPP_
#define OT_NPARTYTRIPLGENERATOR_HPP_

#include "NPartyTripleGenerator.h"

#include "OT/OTExtensionWithMatrix.h"
#include "OT/OTMultiplier.h"
#include "Tools/Subroutines.h"
#include "Protocols/MAC_Check.h"
#include "GC/SemiSecret.h"
#include "GC/SemiPrep.h"

#include "OT/Triple.hpp"
#include "OT/OTMultiplier.hpp"
#include "Protocols/MAC_Check.hpp"
#include "Protocols/SemiInput.hpp"
#include "Protocols/SemiMC.hpp"
#include "Protocols/mac_key.hpp"

#include <sstream>
#include <fstream>
#include <math.h>

template<class T>
void* run_ot_thread(void* ptr)
{
    bigint::init_thread();
    ((OTMultiplierBase*)ptr)->multiply();
    return NULL;
}

/*
 * Copies the relevant base OTs from setup
 * N.B. setup must not be stored as it will be used by other threads
 */
template<class T>
NPartyTripleGenerator<T>::NPartyTripleGenerator(const OTTripleSetup& setup,
        const Names& names, int thread_num, int _nTriples, int nloops,
        MascotParams& machine, mac_key_type mac_key, Player* parentPlayer) :
        OTTripleGenerator<T>(setup, names, thread_num, _nTriples, nloops,
                machine, mac_key, parentPlayer)
{
}

template<class T>
SimpleMascotTripleGenerator<T>::SimpleMascotTripleGenerator(const OTTripleSetup& setup,
        const Names& names, int thread_num, int _nTriples, int nloops,
        MascotParams& machine, mac_key_type mac_key, Player* parentPlayer) :
        NPartyTripleGenerator<T>(setup, names, thread_num, _nTriples, nloops,
                machine, mac_key, parentPlayer)
{
}

template<class T>
MascotTripleGenerator<T>::MascotTripleGenerator(const OTTripleSetup& setup,
        const Names& names, int thread_num, int _nTriples, int nloops,
        MascotParams& machine, mac_key_type mac_key, Player* parentPlayer) :
        SimpleMascotTripleGenerator<T>(setup, names, thread_num, _nTriples, nloops,
                machine, mac_key, parentPlayer)
{
}

template<class T>
Spdz2kTripleGenerator<T>::Spdz2kTripleGenerator(const OTTripleSetup& setup,
        const Names& names, int thread_num, int _nTriples, int nloops,
        MascotParams& machine, mac_key_type mac_key, Player* parentPlayer) :
        NPartyTripleGenerator<T>(setup, names, thread_num, _nTriples, nloops,
                machine, mac_key, parentPlayer)
{
}

template<class T>
OTTripleGenerator<T>::OTTripleGenerator(const OTTripleSetup& setup,
        const Names& names __attribute__((unused)), int thread_num,
        int _nTriples, int nloops,
        MascotParams& machine, mac_key_type mac_key, Player* parentPlayer) :
        globalPlayer(parentPlayer ? *parentPlayer : *new DotsPlayer()),
        parentPlayer(parentPlayer),
        thread_num(thread_num),
        mac_key(mac_key),
        my_num(setup.get_my_num()),
        nloops(nloops),
        nparties(setup.get_nparties()),
        machine(machine),
        MC(0)
{
    nTriplesPerLoop = DIV_CEIL(_nTriples, nloops);
    nTriples = nTriplesPerLoop * nloops;
    field_size = T::open_type::size() * 8;
    nAmplify = machine.amplify ? N_AMPLIFY : 1;
    nPreampTriplesPerLoop = nTriplesPerLoop * nAmplify;

    int n = nparties;
    //baseReceiverInput = machines[0]->baseReceiverInput;
    //baseSenderInputs.resize(n-1);
    //baseReceiverOutputs.resize(n-1);
    nbase = setup.get_nbase();
    baseReceiverInput.resize(nbase);
    baseReceiverOutputs = setup.baseReceiverOutputs;
    baseSenderInputs = setup.baseSenderInputs;
    players.resize(n-1);

    for (int i = 0; i < n-1; i++)
    {
        // i for indexing, other_player is actual number
        int other_player;
        if (i >= my_num)
            other_player = i + 1;
        else
            other_player = i;

        // copy base OT inputs + outputs
        for (int j = 0; j < 128; j++)
        {
            baseReceiverInput.set_bit(j, (unsigned int)setup.get_base_receiver_input(j));
        }

        players[i] = new VirtualTwoPartyPlayer(globalPlayer, other_player);
    }

    pthread_mutex_init(&mutex, 0);
    pthread_cond_init(&ready, 0);

    ot_multipliers.resize(nparties-1);

    for (int i = 0; i < nparties-1; i++)
    {
        ot_multipliers[i] = new_multiplier(i);
        pthread_create(&(ot_multipliers[i]->thread), 0, run_ot_thread<T>, ot_multipliers[i]);
    }

    wait_for_multipliers();
}

template<class T>
OTTripleGenerator<T>::~OTTripleGenerator()
{
    // wait for threads to finish
    for (int i = 0; i < nparties-1; i++)
    {
        ot_multipliers[i]->inbox.stop();
        pthread_join(ot_multipliers[i]->thread, NULL);
#ifdef DEBUG_THREADS
        cout << "OT thread " << i << " finished\n" << flush;
#endif
    }

    for (size_t i = 0; i < ot_multipliers.size(); i++)
        delete ot_multipliers[i];

    for (size_t i = 0; i < players.size(); i++)
        delete players[i];
    //delete nplayer;
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&ready);

    if (parentPlayer != &globalPlayer)
        delete &globalPlayer;
}

template<class T>
typename T::Multiplier* OTTripleGenerator<T>::new_multiplier(int i)
{
    return new typename T::Multiplier(*this, i);
}

template<class T>
void NPartyTripleGenerator<T>::generate()
{
    bigint::init_thread();

    auto& timers = this->timers;
    auto& machine = this->machine;
    auto& my_num = this->my_num;
    auto& thread_num = this->thread_num;
    auto& nTriples = this->nTriples;
    auto& outputFile = this->outputFile;

    timers["Generator thread"].start();

    // add up the shares from each thread and write to file
    stringstream ss;
    ss << machine.prep_data_dir;
    if (machine.generateBits)
        ss << "Bits-";
    else
        ss << "Triples-";
    ss << T::type_short() << "-P" << my_num;
    if (thread_num != 0)
        ss << "-" << thread_num;
    if (machine.output)
    {
        outputFile.open(ss.str().c_str());
        if (machine.generateMACs or not T::clear::invertible)
            file_signature<T>().output(outputFile);
        else
            file_signature<typename T::clear>().output(outputFile);
    }

    if (machine.generateBits)
        generateBits();
    else
        generateTriples();

    timers["Generator thread"].stop();
    if (machine.output)
        cout << "Written " << nTriples << " " << T::type_string() << " outputs to " << ss.str() << endl;
#ifdef VERBOSE_OT
    else
        cerr << "Generated " << nTriples << " " << T::type_string() << " outputs" << endl;
#endif
}

template<class W>
void NPartyTripleGenerator<W>::generateInputs(int player)
{
    typedef typename W::input_type::share_type::open_type T;

    auto nTriplesPerLoop = this->nTriplesPerLoop * 10;
    auto& valueBits = this->valueBits;
    auto& share_prg = this->share_prg;
    auto& ot_multipliers = this->ot_multipliers;
    auto& nparties = this->nparties;
    auto& globalPlayer = this->globalPlayer;

    // extra value for sacrifice
    int toCheck = nTriplesPerLoop
            + DIV_CEIL(W::mac_key_type::size_in_bits(), T::size_in_bits());
    valueBits.resize(1);
    this->signal_multipliers({player, toCheck});
    bool mine = player == globalPlayer.my_num();

    if (mine)
    {
        valueBits[0].resize(toCheck * T::size_in_bits());
        valueBits[0].template randomize_blocks<T>(share_prg);
        this->signal_multipliers({});
    }

    this->wait_for_multipliers();

    GlobalPRNG G(globalPlayer);
    typename W::input_check_type check_sum;
    inputs.resize(toCheck);
    auto mac_key = this->get_mac_key();
    SemiInput<SemiShare<T>> input(0, globalPlayer);
    input.reset_all(globalPlayer);
    vector<T> secrets(toCheck);
    if (mine)
        for (int j = 0; j < toCheck; j++)
        {
            secrets[j] = valueBits[0].template get_portion<T>(j);
            input.add_mine(secrets[j]);
        }
    input.exchange();
    for (int j = 0; j < toCheck; j++)
    {
        T share;
        typename W::mac_type mac_sum;
        share = input.finalize(player);
        if (mine)
        {
            mac_sum = secrets[j] * mac_key;
            for (int i = 0; i < nparties-1; i++)
                mac_sum += (ot_multipliers[i])->input_macs[j];
        }
        else
        {
            int i_thread = player - (player > globalPlayer.my_num() ? 1 : 0);
            mac_sum = (ot_multipliers[i_thread])->input_macs[j];
        }
        inputs[j] = {{share, mac_sum}, secrets[j]};
        auto r = G.get<typename W::input_check_type::share_type>();
        check_sum += typename W::input_check_type(r * share, r * mac_sum);
    }
    inputs.resize(nTriplesPerLoop);

    typename W::input_check_type::MAC_Check MC(mac_key);
    // use zero element because all is perfectly randomized
    MC.set_random_element({});
    MC.POpen(check_sum, globalPlayer);
    MC.Check(globalPlayer);
}

template<class T>
void MascotTripleGenerator<T>::generateBitsGf2n()
{
    auto& bits = this->bits;
    auto& globalPlayer = this->globalPlayer;
    auto& nTriplesPerLoop = this->nTriplesPerLoop;
    auto& valueBits = this->valueBits;

    this->signal_multipliers(DATA_BIT);

    int nBitsToCheck = this->nTriplesPerLoop + this->field_size;
    valueBits.resize(1);
    valueBits[0].resize(ceil(1.0 * nBitsToCheck / 128) * 128);
    bits.resize(nBitsToCheck);
    vector<T> to_open(1);
    vector<typename T::clear> opened(1);
    MAC_Check_<T> MC(this->get_mac_key());

    this->start_progress();

    for (int k = 0; k < this->nloops; k++)
    {
        this->print_progress(k);

        valueBits[0].randomize(this->share_prg);

        this->signal_multipliers({});
        this->timers["Authentication OTs"].start();
        this->wait_for_multipliers();
        this->timers["Authentication OTs"].stop();

        octet seed[SEED_SIZE];
        Create_Random_Seed(seed, this->globalPlayer, SEED_SIZE);
        PRNG G;
        G.SetSeed(seed);

        T check_sum;
        typename T::clear r;
        for (int j = 0; j < nBitsToCheck; j++)
        {
            auto mac_sum = valueBits[0].get_bit(j) ? this->get_mac_key() : 0;
            for (int i = 0; i < this->nparties-1; i++)
                mac_sum += this->ot_multipliers[i]->macs[0][j];
            bits[j].set_share(valueBits[0].get_bit(j));
            bits[j].set_mac(mac_sum);
            r.randomize(G);
            check_sum += r * bits[j];
        }
        bits.resize(nTriplesPerLoop);

        to_open[0] = check_sum;
        MC.POpen_Begin(opened, to_open, globalPlayer);
        MC.POpen_End(opened, to_open, globalPlayer);
        MC.Check(globalPlayer);

        if (this->machine.output)
            for (int j = 0; j < nTriplesPerLoop; j++)
                bits[j].output(this->outputFile, false);
   }
}

template<class T>
void MascotTripleGenerator<T>::generateBits()
{
    if (T::clear::characteristic_two)
        generateBitsGf2n();
    else
        this->generateTriples();
}

template<class T>
void Spdz2kTripleGenerator<T>::generateTriples()
{
	const int K = T::k;
	const int S = T::s;
	auto& uncheckedTriples = this->uncheckedTriples;

	auto& timers = this->timers;
	auto& nTriplesPerLoop = this->nTriplesPerLoop;
	auto& valueBits = this->valueBits;
	auto& share_prg = this->share_prg;
	auto& ot_multipliers = this->ot_multipliers;
	auto& nparties = this->nparties;
	auto& globalPlayer = this->globalPlayer;
	auto& nloops = this->nloops;
	auto& b_padded_bits = this->b_padded_bits;

	this->signal_multipliers(DATA_TRIPLE);

	const int TAU = Spdz2kMultiplier<K, S>::TAU;
	const int TAU_ROUNDED = (TAU + 7) / 8 * 8;
	valueBits.resize(3);
	for (int i = 0; i < 2; i++)
		valueBits[2*i].resize(max(2 * 8 * Z2<K + 2 * S>::N_BYTES, TAU_ROUNDED) * nTriplesPerLoop);
	valueBits[1].resize(8 * Z2<K + S>::N_BYTES * (nTriplesPerLoop + 1));
	b_padded_bits.resize(8 * Z2<K + 2 * S>::N_BYTES * (nTriplesPerLoop + 1));
	vector< PlainTriple_<Z2<K + 2 * S>, Z2<K + S>, 2> > amplifiedTriples(nTriplesPerLoop);
	uncheckedTriples.resize(nTriplesPerLoop);
	typename T::prep_check_type::MAC_Check MC(this->get_mac_key());

	this->start_progress();

	for (int k = 0; k < nloops; k++)
	{
		this->print_progress(k);

		for (int j = 0; j < 2; j++)
			valueBits[j].randomize(share_prg);

		for (int j = 0; j < nTriplesPerLoop + 1; j++)
		{
			Z2<K + S> b(valueBits[1].get_ptr_to_byte(j, Z2<K + S>::N_BYTES));
			b_padded_bits.set_portion(j, Z2<K + 2 * S>(b));
		}

		timers["OTs"].start();
		this->signal_multipliers({});
		this->wait_for_multipliers();
		timers["OTs"].stop();

		octet seed[SEED_SIZE];
		Create_Random_Seed(seed, globalPlayer, SEED_SIZE);
		PRNG G;
		G.SetSeed(seed);

		BitVector aBits = valueBits[0];

		for (int j = 0; j < nTriplesPerLoop; j++)
		{
			BitVector a(aBits.get_ptr_to_bit(j, TAU_ROUNDED), TAU);
			Z2<K + S> b(valueBits[1].get_ptr_to_byte(j, Z2<K + S>::N_BYTES));
			Z2kRectangle<TAU, K + S> c;
			c.mul(a, b);
			timers["Triple computation"].start();
			for (int i = 0; i < nparties-1; i++)
			{
				c += ot_multipliers[i]->c_output[j];
			}

#ifdef DEBUG_SPDZ2K
			for (int l = 0; l < c.N_ROWS; l++)
			{
				Z2<K + S> z = fake.POpen({c.rows[l], {}}, globalPlayer);
				auto x = fake.POpen({a.get_bit(l), {}}, globalPlayer);
				auto y = fake.POpen({b, {}}, globalPlayer);
				Z2<K + S> zz = x * y;
				if (z != zz)
				{
					cout << dec << j << " " << l << " " << hex << z << " " << zz
							<< " " << y << " " << x << " " << a.get_byte(l / 8) << endl;
				}
			}
#endif

			timers["Triple computation"].stop();
			amplifiedTriples[j].amplify(a, b, c, G);
			amplifiedTriples[j].to(valueBits, j);
		}

		this->signal_multipliers({});
		this->wait_for_multipliers();

		for (int j = 0; j < nTriplesPerLoop; j++)
		{
			uncheckedTriples[j].from(amplifiedTriples[j], j, *this);
		}

		// we can skip the consistency check since we're doing a mac-check next
		// get piggy-backed random value
		Z2<K + 2 * S> r_share = b_padded_bits.get_ptr_to_byte(nTriplesPerLoop, Z2<K + 2 * S>::N_BYTES);
		Z2<K + 2 * S> r_mac;
		r_mac = (r_share * this->get_mac_key());
		for (int i = 0; i < this->nparties-1; i++)
			r_mac += (ot_multipliers[i])->macs.at(1).at(nTriplesPerLoop);
		Share<Z2<K + 2 * S>> r;
		r.set_share(r_share);
		r.set_mac(r_mac);

		MC.set_random_element(r);
		sacrificeZ2k(MC, G);
	}
}

template<class U>
void OTTripleGenerator<U>::generatePlainTriples()
{
    machine.set_passive();
    machine.output = false;
    signal_multipliers(DATA_TRIPLE);

    valueBits.resize(3);
    for (int i = 0; i < 3; i++)
        valueBits[i].resize(field_size * nPreampTriplesPerLoop);

    start_progress();
    for (int i = 0; i < nloops; i++)
        plainTripleRound(i);
}

template<class T>
void OTTripleGenerator<T>::generatePlainBits()
{
    assert(ot_multipliers.size() == 1);

    machine.set_passive();
    machine.output = false;

    int n = multiple_minimum(100 * nPreampTriplesPerLoop,
            T::open_type::size_in_bits());

    valueBits.resize(1);
    valueBits[0].resize(n);
    valueBits[0].randomize(share_prg);

    signal_multipliers(DATA_BIT);

    wait_for_multipliers();
    plainBits.clear();

    typename T::open_type two = 2;

    for (int j = 0; j < n; j++)
    {
        if (j % T::open_type::size_in_bits() < T::open_type::length())
        {
            bool b = valueBits[0].get_bit(j);
            plainBits.push_back({b, b});
            plainBits.back().first += ot_multipliers[0]->c_output[j] * two;
        }
    }
}

template<class T>
void OTTripleGenerator<T>::generateMixedTriples()
{
    assert(ot_multipliers.size() == 1);

    machine.set_passive();
    machine.output = false;

    int n = multiple_minimum(100 * nPreampTriplesPerLoop,
            T::open_type::size_in_bits());

    valueBits.resize(2);
    valueBits[0].resize(n);
    valueBits[0].randomize(share_prg);
    valueBits[1].resize(n * T::open_type::N_BITS);
    valueBits[1].randomize(share_prg);

    signal_multipliers(DATA_MIXED);

    wait_for_multipliers();
    mixedTriples.clear();

    for (int j = 0; j < n; j++)
    {
        auto a = valueBits[0].get_bit(j);
        auto b = valueBits[1].template get_portion<typename T::open_type>(j);
        auto c = a ? b : typename T::open_type();
        for (auto& x : ot_multipliers)
            c += x->c_output[j];
        mixedTriples.push_back({{a, b, c}});
    }
}

template<class U>
void OTTripleGenerator<U>::plainTripleRound(int k)
{
    typedef typename U::open_type T;

    if (not (machine.amplify or machine.output))
        plainTriples.resize(nPreampTriplesPerLoop);

    print_progress(k);

    for (int j = 0; j < 2; j++)
        valueBits[j].template randomize_blocks<T>(share_prg);

    timers["OTs"].start();
    for (int i = 0; i < nparties-1; i++)
        ot_multipliers[i]->inbox.push({});
    this->wait_for_multipliers();
    timers["OTs"].stop();

    for (int j = 0; j < nPreampTriplesPerLoop; j++)
    {
        T a;
        a.assign((char*)valueBits[0].get_ptr() + j * T::size());
        T b;
        b.assign((char*)valueBits[1].get_ptr() + j / nAmplify * T::size());
        T c = a * b;
        timers["Triple computation"].start();
        for (int i = 0; i < nparties-1; i++)
        {
            c += ot_multipliers[i]->c_output[j];
        }
        timers["Triple computation"].stop();
        if (machine.amplify)
        {
            preampTriples[j/nAmplify].a[j%nAmplify] = a;
            preampTriples[j/nAmplify].b = b;
            preampTriples[j/nAmplify].c[j%nAmplify] = c;
        }
        else if (machine.output)
        {
            timers["Writing"].start();
            a.output(outputFile, false);
            b.output(outputFile, false);
            c.output(outputFile, false);
            timers["Writing"].stop();
        }
        else
        {
            plainTriples[j] = {{a, b, c}};
        }

#ifdef DEBUG_MASCOT
        cout << "lengths ";
        for (int i = 0; i < 3; i++)
            cout << valueBits[i].size() << " ";
        cout << endl;

        auto& P = globalPlayer;
        SemiMC<SemiShare<T>> MC;

        auto aa = MC.open(a, P);
        auto bb = MC.open(b, P);
        auto cc = MC.open(c, P);
        if (cc != aa * bb)
        {
            cout << j << " " << cc << " != " << aa << " * " << bb << ", diff " <<
                    (cc - aa * bb) << endl;
            cout << "OT output " << ot_multipliers[0]->c_output[j] << endl;
            assert(cc == aa * bb);
        }
#endif
    }

#ifdef DEBUG_MASCOT
    cout << "plain triple round done" << endl;
#endif
}

template<class U>
void SimpleMascotTripleGenerator<U>::generateTriples()
{
    typedef typename U::open_type T;

    auto& timers = this->timers;
    auto& machine = this->machine;
    auto& nTriplesPerLoop = this->nTriplesPerLoop;
    auto& valueBits = this->valueBits;
    auto& ot_multipliers = this->ot_multipliers;
    auto& nparties = this->nparties;
    auto& globalPlayer = this->globalPlayer;
    auto& nloops = this->nloops;
    auto& preampTriples = this->preampTriples;
    auto& outputFile = this->outputFile;
    auto& field_size = this->field_size;
    auto& nPreampTriplesPerLoop = this->nPreampTriplesPerLoop;
    auto& uncheckedTriples = this->uncheckedTriples;

	for (int i = 0; i < nparties-1; i++)
	    ot_multipliers[i]->inbox.push(DATA_TRIPLE);

    valueBits.resize(3);
    for (int i = 0; i < 2; i++)
        valueBits[2*i].resize(field_size * nPreampTriplesPerLoop);
    valueBits[1].resize(field_size * nTriplesPerLoop);
    vector< PlainTriple<T,2> > amplifiedTriples;
    MAC_Check MC(this->get_mac_key());

    if (machine.amplify)
        preampTriples.resize(nTriplesPerLoop);
    if (machine.generateMACs)
      {
	amplifiedTriples.resize(nTriplesPerLoop);
      }

    uncheckedTriples.resize(nTriplesPerLoop);

    this->start_progress();

    for (int k = 0; k < nloops; k++)
    {
        this->plainTripleRound();

        if (machine.amplify)
        {
            PRNG G;
            if (machine.fiat_shamir and nparties == 2)
                ot_multipliers[0]->otCorrelator.common_seed(G);
            else
            {
                octet seed[SEED_SIZE];
                Create_Random_Seed(seed, globalPlayer, SEED_SIZE);
                G.SetSeed(seed);
            }
            for (int iTriple = 0; iTriple < nTriplesPerLoop; iTriple++)
            {
                PlainTriple<T,2> triple;
                triple.amplify(preampTriples[iTriple], G);

                if (machine.generateMACs)
                    amplifiedTriples[iTriple] = triple;
                else if (machine.output)
                {
                    timers["Writing"].start();
                    triple.output(outputFile);
                    timers["Writing"].stop();
                }
                else
                    for (int i = 0; i < 3; i++)
                        uncheckedTriples[iTriple].byIndex(i, 0).set_share(triple.byIndex(i, 0));
            }

            if (machine.generateMACs)
            {
                for (int iTriple = 0; iTriple < nTriplesPerLoop; iTriple++)
                    amplifiedTriples[iTriple].to(valueBits, iTriple,
                            machine.check ? 2 : 1);

                for (int i = 0; i < nparties-1; i++)
                    ot_multipliers[i]->inbox.push({});
                timers["Authentication OTs"].start();
                this->wait_for_multipliers();
                timers["Authentication OTs"].stop();

                for (int iTriple = 0; iTriple < nTriplesPerLoop; iTriple++)
                {
                    uncheckedTriples[iTriple].from(amplifiedTriples[iTriple], iTriple, *this);

                    if (!machine.check and machine.output)
                    {
                        timers["Writing"].start();
                        amplifiedTriples[iTriple].output(outputFile);
                        timers["Writing"].stop();
                    }
                }

                if (machine.check)
                {
                    sacrifice(this->MC ? *this->MC : MC, G);
                }
            }
        }
    }
}

template<class T>
void MascotTripleGenerator<T>::sacrifice(typename T::MAC_Check& MC, PRNG& G)
{
    auto& machine = this->machine;
    auto& nTriplesPerLoop = this->nTriplesPerLoop;
    auto& globalPlayer = this->globalPlayer;
    auto& outputFile = this->outputFile;
    auto& uncheckedTriples = this->uncheckedTriples;

    check_field_size<typename T::clear>();

    vector<T> maskedAs(nTriplesPerLoop);
    vector<TripleToSacrifice<T> > maskedTriples(nTriplesPerLoop);
    for (int j = 0; j < nTriplesPerLoop; j++)
    {
        maskedTriples[j].template prepare_sacrifice<T>(uncheckedTriples[j], G);
        maskedAs[j] = maskedTriples[j].a[0];
    }

    vector<open_type> openedAs(nTriplesPerLoop);
    MC.POpen_Begin(openedAs, maskedAs, globalPlayer);
    MC.POpen_End(openedAs, maskedAs, globalPlayer);

#ifdef DEBUG_MASCOT
    MC.Check(globalPlayer);
    auto& P = globalPlayer;

    for (int j = 0; j < nTriplesPerLoop; j++)
        for (int i = 0; i < 2; i++)
        {
            auto a = MC.open(uncheckedTriples[j].a[i], P);
            auto b = MC.open(uncheckedTriples[j].b, P);
            auto c = MC.open(uncheckedTriples[j].c[i], P);
            if (c != a * b)
            {
                cout << c << " != " << a << " * " << b << ", diff " << hex <<
                        (c - a * b) << endl;
                assert(c == a * b);
            }
        }

    MC.Check(globalPlayer);
#endif

    for (int j = 0; j < nTriplesPerLoop; j++) {
        MC.AddToCheck(maskedTriples[j].computeCheckShare(openedAs[j]), 0,
                globalPlayer);
    }

    MC.Check(globalPlayer);

    if (machine.generateBits)
        generateBitsFromTriples(MC, outputFile, typename T::clear());
    else
        if (machine.output)
            for (int j = 0; j < nTriplesPerLoop; j++)
                uncheckedTriples[j].output(outputFile, 1);
}

template<class W>
template<class U>
void Spdz2kTripleGenerator<W>::sacrificeZ2k(U& MC, PRNG& G)
{
    typedef sacri_type T;
    typedef open_type V;
    typedef typename W::prep_check_type prep_check_type;

    auto& machine = this->machine;
    auto& nTriplesPerLoop = this->nTriplesPerLoop;
    auto& globalPlayer = this->globalPlayer;
    auto& outputFile = this->outputFile;
    auto& uncheckedTriples = this->uncheckedTriples;

    vector<prep_check_type> maskedAs(nTriplesPerLoop);
    vector<TripleToSacrifice<Share<T>> > maskedTriples(nTriplesPerLoop);
    for (int j = 0; j < nTriplesPerLoop; j++)
    {
        // compute [p] = t * [a] - [ahat]
        // and first part of [sigma], i.e., t * [c] - [chat] 
        maskedTriples[j].template prepare_sacrifice<W>(uncheckedTriples[j], G);
        maskedAs[j] = maskedTriples[j].a[0];
        // enough randomness in values
        MC.set_random_element({});
    }

    vector<T> openedAs(nTriplesPerLoop);
    MC.POpen_Begin(openedAs, maskedAs, globalPlayer);
    MC.POpen_End(openedAs, maskedAs, globalPlayer);

    vector<prep_check_type> sigmas;
    for (int j = 0; j < nTriplesPerLoop; j++) {
        // compute t * [c] - [chat] - [b] * p
        sigmas.push_back(maskedTriples[j].computeCheckShare(V(openedAs[j])));
        // enough randomness in values
        MC.set_random_element({});
    }
    vector<T> open_sigmas;
    
    MC.POpen_Begin(open_sigmas, sigmas, globalPlayer);
    MC.POpen_End(open_sigmas, sigmas, globalPlayer);
    MC.Check(globalPlayer);

    for (int j = 0; j < nTriplesPerLoop; j++) {
        if (V(open_sigmas[j]) != 0)
        {
            throw runtime_error("sacrifice fail");
        }
    }
    
    if (machine.generateBits)
        throw not_implemented();
    else
        if (machine.output)
            for (int j = 0; j < nTriplesPerLoop; j++)
                uncheckedTriples[j].template reduce<W>().output(outputFile, 1);
}

template<class T>
template<int X, int L>
void MascotTripleGenerator<T>::generateBitsFromTriples(MAC_Check& MC,
        ofstream& outputFile, gfp_<X, L>)
{
    typedef gfp_<X, L> gfp1;
    auto& triples = this->uncheckedTriples;
    auto& nTriplesPerLoop = this->nTriplesPerLoop;
    auto& globalPlayer = this->globalPlayer;
    vector< Share<gfp1> > a_plus_b(nTriplesPerLoop), a_squared(nTriplesPerLoop);
    for (int i = 0; i < nTriplesPerLoop; i++)
        a_plus_b[i] = triples[i].a[0] + triples[i].b;
    vector<gfp1> opened(nTriplesPerLoop);
    MC.POpen_Begin(opened, a_plus_b, globalPlayer);
    MC.POpen_End(opened, a_plus_b, globalPlayer);
    for (int i = 0; i < nTriplesPerLoop; i++)
        a_squared[i] = triples[i].a[0] * opened[i] - triples[i].c[0];
    MC.POpen_Begin(opened, a_squared, globalPlayer);
    MC.POpen_End(opened, a_squared, globalPlayer);
    MC.Check(globalPlayer);
    auto one = Share<gfp1>::constant(1, globalPlayer.my_num(), MC.get_alphai());
    bits.clear();
    for (int i = 0; i < nTriplesPerLoop; i++)
    {
        gfp1 root = opened[i].sqrRoot();
        if (root.is_zero())
            continue;
        Share<gfp1> bit = (triples[i].a[0] / root + one) / gfp1(2);
        if (this->machine.output)
            bit.output(outputFile, false);
        else
            bits.push_back(bit);
    }
}

template<class T>
template<class U>
void MascotTripleGenerator<T>::generateBitsFromTriples(MAC_Check&, ofstream&, U)
{
    throw how_would_that_work();
}

template <class T>
void OTTripleGenerator<T>::start_progress()
{
    wait_for_multipliers();
    lock();
    signal();
    wait();
    gettimeofday(&last_lap, 0);
}

template<class T>
void OTTripleGenerator<T>::print_progress(int k)
{
    if (thread_num == 0 && my_num == 0)
    {
        struct timeval stop;
        gettimeofday(&stop, 0);
        if (timeval_diff_in_seconds(&last_lap, &stop) > 1)
        {
            double diff = timeval_diff_in_seconds(&machine.start, &stop);
            double throughput = k * nTriplesPerLoop * machine.nthreads / diff;
            double remaining = diff * (nloops - k) / k;
            cout << k << '/' << nloops << ", throughput: " << throughput
                    << ", time left: " << remaining << ", elapsed: " << diff
                    << ", estimated total: " << (diff + remaining) << endl;
            last_lap = stop;
        }
    }
}

inline
void GeneratorThread::lock()
{
    pthread_mutex_lock(&mutex);
}

inline
void GeneratorThread::unlock()
{
    pthread_mutex_unlock(&mutex);
}

inline
void GeneratorThread::signal()
{
    pthread_cond_signal(&ready);
}

inline
void GeneratorThread::wait()
{
    if (multi_threaded)
        pthread_cond_wait(&ready, &mutex);
}

template<class T>
void OTTripleGenerator<T>::signal_multipliers(MultJob job)
{
    for (int i = 0; i < nparties-1; i++)
        ot_multipliers[i]->inbox.push(job);
}

template<class T>
void OTTripleGenerator<T>::wait_for_multipliers()
{
    for (int i = 0; i < nparties-1; i++)
        ot_multipliers[i]->outbox.pop();
}

template<class T>
void OTTripleGenerator<T>::run_multipliers(MultJob job)
{
    signal_multipliers(job);
    wait_for_multipliers();
}

#endif
