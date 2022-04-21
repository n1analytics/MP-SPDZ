/*
 * fake-spdz-ecdsa-party.cpp
 *
 */
#define INPUTSIZE 10

#include "Networking/Server.h"
#include "Networking/CryptoPlayer.h"
#include "Math/gfp.h"
#include "ECDSA/P256Element.h"
#include "Protocols/SemiShare.h"
#include "Processor/BaseMachine.h"

#include "ECDSA/preprocessing.hpp"
#include "ECDSA/sign.hpp"
#include "Protocols/Beaver.hpp"
#include "Protocols/ProtocolSet.h"
#include "Protocols/fake-stuff.hpp"
#include "Protocols/MascotPrep.hpp"
#include "Processor/Processor.hpp"
#include "Processor/Data_Files.hpp"
#include "Processor/Input.hpp"
#include "GC/TinyPrep.hpp"
#include "GC/VectorProtocol.hpp"
#include "GC/CcdPrep.hpp"
#include "Tools/Bundle.h"

#include <assert.h>

template<template<class U> class T>
class PCIInput
{
public:
    P256Element::Scalar sk;
    P256Element PK;
    EcSignature signature;
};

// ask abhijit da about this
template<template<class U> class T>
void run(int argc, const char** argv)
{
    ez::ezOptionParser opt;
    EcdsaOptions opts(opt, argc, argv);
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Use SimpleOT instead of OT extension", // Help description.
            "-S", // Flag token.
            "--simple-ot" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Don't check correlation in OT extension (only relevant with MASCOT)", // Help description.
            "-U", // Flag token.
            "--unchecked-correlation" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Fewer rounds for authentication (only relevant with MASCOT)", // Help description.
            "-A", // Flag token.
            "--auth-fewer-rounds" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Use Fiat-Shamir for amplification (only relevant with MASCOT)", // Help description.
            "-H", // Flag token.
            "--fiat-shamir" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Skip sacrifice (only relevant with MASCOT)", // Help description.
            "-E", // Flag token.
            "--embrace-life" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "No MACs (only relevant with MASCOT; implies skipping MAC checks)", // Help description.
            "-M", // Flag token.
            "--no-macs" // Flag token.
    );


    // Setup network with two players
    Names N(opt, argc, argv, 2);
    
    // Setup PlainPlayer
    PlainPlayer P(N, "ecdsa");

    // Initialize curve and field
    // Initializes the field order to same as curve order 
    P256Element::init();
    // Initialize scalar:next with same order as field order. ??
    P256Element::Scalar::next::init_field(P256Element::Scalar::pr(), false);
    int prime_length = P256Element::Scalar::length();

    typedef T<P256Element::Scalar> scalarShare;
    // typedef T<P256Element> ecShare;

    BaseMachine machine;
    machine.ot_setups.push_back({P, true});

    // Generate secret keys and signatures with them
    vector<PCIInput<T>> pciinputs;
    P256Element::Scalar sk;

    SeededPRNG G;
    unsigned char* message = (unsigned char*)"this is a sample claim"; // 22
    
    for (int i = 0; i < INPUTSIZE; i++)
    {
        // chose random sk
        sk.randomize(G);

        // create pk
        P256Element Pk(sk);

        // sign
        EcSignature signature = sign(message, 22, sk);
 
        pciinputs.push_back({sk, Pk, signature});
    }

    // Protocol Sets
    ProtocolSetup<scalarShare> protocolSetup(P, prime_length);
    ProtocolSet<scalarShare> protocolSet(P, protocolSetup);
    int thisplayer = N.my_num();

    // Input Shares
    vector<scalarShare> inputs_shares[2];


    // Give Input
    typename scalarShare::Input input = protocolSet.input;

    input.reset_all(P);
    for (int i = 0; i < INPUTSIZE; i++)
        input.add_from_all(pciinputs[i].sk);
    input.exchange();
    for (int i = 0; i < INPUTSIZE; i++)
    {
        // shares of party A
        inputs_shares[0].push_back(input.finalize(0));

        // shares of party B
        inputs_shares[1].push_back(input.finalize(1));
    }
    cout << "---- inputs shared ----" << thisplayer << endl;

    // output
    typename scalarShare::clear result;
    auto& output = protocolSet.output;
    output.init_open(P);
    output.prepare_open(inputs_shares[0][0]);
    output.exchange(P);
    result = output.finalize_open();
    cout << "-->" << pciinputs[0].sk << endl;
    cout << "-->" << result << endl;
    protocolSet.check();
    // OnlineOptions::singleton.batch_size = 1;

    // // Direct_MC is a subtype of class pShare -- Direct_MC in Share.h
    // typename pShare::Direct_MC MCp(keyp);


    // ArithmeticProcessor _({}, 0);

    // // TriplePrep in Share.h
    // typename pShare::TriplePrep sk_prep(0, usage);

    // SubProcessor<pShare> sk_proc(_, MCp, sk_prep, P);
    // pShare sk, __;

    // // synchronize
    // Bundle<octetStream> bundle(P);
    // P.unchecked_broadcast(bundle);

    // Timer timer;
    // timer.start();
    // auto stats = P.total_comm();

    

    // // let the size of inputs be known.

    //     // Generate Inputs
    // // Each party needs to generate a set of secret keys
    // //  Sign message with the keys

    // // Share the public keys and the signatures


    // // Generate secret key
    // sk_prep.get_two(DATA_INVERSE, sk, __);
    // cout << "Secret key generation took " << timer.elapsed() * 1e3 << " ms" << endl;
    // (P.total_comm() - stats).print(true);

    // cout << "-----------------------------------" << endl;

    // // Configure
    // OnlineOptions::singleton.batch_size = (1 + pShare::Protocol::uses_triples) * n_tuples;
    // typename pShare::TriplePrep prep(0, usage);
    // prep.params.correlation_check &= not opt.isSet("-U");
    // prep.params.fewer_rounds = opt.isSet("-A");
    // prep.params.fiat_shamir = opt.isSet("-H");
    // prep.params.check = not opt.isSet("-E");
    // prep.params.generateMACs = not opt.isSet("-M");
    // opts.check_beaver_open &= prep.params.generateMACs;
    // opts.check_open &= prep.params.generateMACs;
    // // Subprocessor
    // SubProcessor<pShare> proc(_, MCp, prep, P);
    // typename pShare::prep_type::Direct_MC MCpp(keyp);
    // prep.triple_generator->MC = &MCpp;

    // bool prep_mul = not opt.isSet("-D");
    // prep.params.use_extension = not opt.isSet("-S");
    // vector<EcTuple<T>> tuples;


    // cout << "---------------- INPUT PROTOCOL -----------------" << endl;
    // int prime_length = P256Element::Scalar::length();

    // ProtocolSetup<pShare> setup(P, prime_length);

    // ProtocolSet<pShare> set(P, setup);
    // auto& inputProtocol = set.input;

    // inputProtocol.reset_all(P);
    // for (int i = 0; i < N.num_players(); i++)
    //     inputProtocol.add_from_all(i);
    // inputProtocol.exchange();


    // cout << " ----------------- preprocessing ------------" << endl;
    // // Preprocessing
    // preprocessing(tuples, n_tuples, sk, proc, opts);
    // //check(tuples, sk, keyp, P);

  

    // cout << "---------------------------------" << endl;

    // sign_benchmark(tuples, sk, MCp, P, opts, prep_mul ? 0 : &proc);
}
