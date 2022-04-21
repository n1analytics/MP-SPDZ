/*
 * paper-example.cpp
 *
 * Working example similar to Figure 2 in https://eprint.iacr.org/2020/521
 *
 */

#define NO_MIXED_CIRCUITS

#include "Math/gfp.hpp"
#include "Machines/SPDZ.hpp"
#include "Machines/MalRep.hpp"
#include "Machines/ShamirMachine.hpp"
#include "Machines/Semi2k.hpp"
#include "Protocols/CowGearShare.h"
#include "Protocols/CowGearPrep.hpp"
#include "Protocols/ProtocolSet.h"

template<class T>
void run(char** argv, int prime_length);

int main(int argc, char** argv)
{
    // bit length of prime
    const int prime_length = 128;

    // compute number of 64-bit words needed
    const int n_limbs = (prime_length + 63) / 64;

    // need player number and number of players
    if (argc < 3)
    {
        cerr << "Usage: " << argv[0] << "<my number: 0/1/...> <total number of players> [protocol [threshold]]" << endl;
        exit(1);
    }

    string protocol = "MASCOT";
    if (argc > 3)
        protocol = argv[3];

    if (protocol == "MASCOT")
        run<Share<gfp_<0, n_limbs>>>(argv, prime_length);
    else if (protocol == "CowGear")
        run<CowGearShare<gfp_<0, n_limbs>>>(argv, prime_length);
    else if (protocol == "SPDZ2k")
        run<Spdz2kShare<64, 64>>(argv, 0);
    else if (protocol == "Semi2k")
        run<Semi2kShare<64>>(argv, 0);
    else if (protocol == "Shamir" or protocol == "MalShamir")
    {
        int nparties = (atoi(argv[2]));
        int threshold = (nparties - 1) / 2;
        if (argc > 4)
            threshold = atoi(argv[4]);
        assert(2 * threshold < nparties);
        ShamirOptions::s().threshold = threshold;
        ShamirOptions::s().nparties = nparties;

        if (protocol == "Shamir")
            run<ShamirShare<gfp_<0, n_limbs>>>(argv, prime_length);
        else
            run<MaliciousShamirShare<gfp_<0, n_limbs>>>(argv, prime_length);
    }
    else
    {
        cerr << "Unknown protocol: " << protocol << endl;
        exit(1);
    }
}

template<class T>
void run(char** argv, int prime_length)
{
    // set up networking on localhost
    int my_number = atoi(argv[1]);
    int n_parties = atoi(argv[2]);
    int port_base = 9999;
    Names N(my_number, n_parties, "localhost", port_base);
    PlainPlayer P(N);

    // protocol setup (domain, MAC key if needed etc)
    ProtocolSetup<T> setup(P, prime_length);

    // set of protocols (input, multiplication, output)
    ProtocolSet<T> set(P, setup);
    auto& input = set.input;
    // auto& protocol = set.protocol;
    auto& output = set.output;

    int n = 1000;
    vector<T> a(n), b(n);
    typename T::clear result;

    input.reset_all(P);
    for (int i = 0; i < n; i++)
        input.add_from_all(i);
    input.exchange();
    for (int i = 0; i < n; i++)
    {
        a[i] = input.finalize(0);
        b[i] = input.finalize(1);
    }

    // T c;
    // protocol.init_dotprod();
    // for (int i = 0; i < n; i++)
    //     protocol.prepare_dotprod(a[i], b[i]);
    // protocol.next_dotprod();
    // protocol.exchange();
    // c = protocol.finalize_dotprod(n);

    // protocol check before revealing results
    set.check();

    output.init_open(P);
    output.prepare_open(a[0]);
    output.exchange(P);
    result = output.finalize_open();

    cout << "result: " << result << endl;

    // result check after opening
    set.check();
}
