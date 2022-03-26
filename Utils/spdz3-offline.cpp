/*
 * spdz3-offline.cpp
 *
 */

#include <signal.h>
#include <stdexcept>
using namespace std;

#include "FHEOffline/DataSetup.h"
#include "FHEOffline/DistKeyGen.h"
#include "FHEOffline/EncCommit.h"
#include "FHEOffline/Producer.h"
#include "Networking/Server.h"
#include "FHE/NTL-Subs.h"
#include "Tools/ezOptionParser.h"
#include "Tools/mkpath.h"
#include "Tools/Signal.h"
#include "Math/Setup.h"

#include "Protocols/MAC_Check.hpp"
#include "Math/gfp.hpp"

class Spdz2
{
public:
    int sec;
    int covert;
    Names N;
    bool stop_requested;
    bool minimal;
    DataSetup setup;
    int prime_length, gf2n_length;

    // constructor of spdz2
    Spdz2() : sec(40), covert(2), stop_requested(false),
            minimal(false),
            prime_length(128), gf2n_length(40) {}

    void stop()
    {
        cout << "Stopping production..." << endl;
        stop_requested = true;
    }
};

template <class FD>
class Spdz2SetupThread;

template <class FD>
class Spdz2GeneratorThread
{
public:
    Spdz2SetupThread<FD>& setup_thread;
    Producer<FD>& producer;
    pthread_t thread;
    int id;

    Spdz2GeneratorThread(Spdz2SetupThread<FD>& setup, Producer<FD>& producer, int id) :
            setup_thread(setup), producer(producer), thread(0), id(id)
    {
    }

    void* run()
    {
        PartSetup<FD>& setup = setup_thread.setup;
        const Spdz2& spdz2 = setup_thread.spdz2;
        PlainPlayer P(spdz2.N, ((id + 1) << 8) + FD::T::type_char());
        EncCommit_<FD> EC(P, setup.pk, spdz2.covert, producer.required_randomness());
        DistDecrypt<FD> dd(P, setup.sk, setup.pk, setup.FieldD);
        MAC_Check<typename FD::T> MC(setup.alphai);
        string data_type = producer.data_type();
        transform(data_type.begin(), data_type.end(), data_type.begin(), ::tolower);
        cout << "Starting to produce " << FD::T::type_string() << " " << data_type << endl;
        int total = 0;
        Timer timer;
        timer.start();
        vector<octetStream> os(P.num_players());
        while (true)
        {
            bool stop = spdz2.stop_requested or (spdz2.minimal and total > 0);
            // see if all happy to continue
            os[P.my_num()].reset_write_head();
            os[P.my_num()].store_int(stop, 1);
            P.Broadcast_Receive(os);
            for (auto& o : os)
                if(o.get_int(1))
                    stop = true;
            if (stop)
                break;

            producer.run(P, setup.pk, setup.calpha, EC, dd, setup.alphai);
            producer.sacrifice(P, MC);
            total += producer.num_slots();
            cout << "Produced " << total << " " << FD::T::type_string() << " "
                    << data_type << ", " << total / timer.elapsed()
                    << " per second" << endl;
        }
        MC.Check(P);
        cout << "Finished producing " << FD::T::type_string() << " " << data_type << endl;
        return 0;
    }
};

template <class FD>
void* run_producer(void* thread)
{
    return ((Spdz2GeneratorThread<FD>*)thread)->run();
}

template <class FD>
class Spdz2SetupThread
{
public:
    PartSetup<FD> setup;
    int plaintext_length;
    const Spdz2& spdz2;
    Signal signal;

    // spdz协议运行的构造函数明文的长度
    Spdz2SetupThread(int plaintext_length, const Spdz2& spdz2) : plaintext_length(plaintext_length), spdz2(spdz2)
    {
        signal.lock();
    }

    void* run()
    {
        // FD::T::tyoe_char() = 'p' 素数域
        PlainPlayer P(spdz2.N, FD::T::type_char());
        // 创建公私钥，mac认证密钥的承诺
        setup.generate_setup(P.num_players(), plaintext_length, spdz2.sec, 0, false);
        // 全局共享私钥，并让公钥成为一个全局密钥
        Run_Gen_Protocol(setup.pk, setup.sk, P, spdz2.covert, false);
        // 生成MAC密钥
        generate_mac_key(setup.alphai, setup.calpha, setup.FieldD, setup.pk, P, spdz2.covert);
        signal.lock();
        signal.broadcast();
        signal.unlock();

        string dir = get_prep_sub_dir<Share<typename FD::T>>(P.num_players());

//        TripleProducer_<FD> ff(setup.FieldD, P.my_num(), 0, true, dir);
//        for (unsigned int i=0; i<4; i++)
//        {
//            cout << i << ":::::" << ff.ai[i] << endl;
//        }
//        cout << "prep_dir: " << dir << endl;


        Producer<FD>* producers[] =
        {
            // 初始化三元组，和用于牺牲的三元组 计算域 计算方id
            new TripleProducer_<FD>(setup.FieldD, P.my_num(), 0, true, dir),
//            new SquareProducer<FD>(setup.FieldD, P.my_num(), 0, true, dir),
//            new_bit_producer(setup.FieldD, P, setup.pk, spdz2.covert, true, 0, true, dir),
//            new InverseProducer<FD>(setup.FieldD, P.my_num(), 0, true, true, dir),
        };
        vector<Spdz2GeneratorThread<FD>*> generators;
        for (int i = 0; i < 4; i++)
        {
            generators.push_back(new Spdz2GeneratorThread<FD>(*this, *producers[i], i));
            if (not spdz2.minimal)
                pthread_create(&generators[i]->thread, 0, run_producer<FD>, generators[i]);
        }
        for (int i = 0; i < 4; i++)
        {
            if (spdz2.minimal)
                generators[i]->run();
            else
                pthread_join(generators[i]->thread, 0);
            delete generators[i];
            delete producers[i];
        }
        return 0;
    }
};

template <class FD>
void* run_setup(void* setup)
{
    return ((Spdz2SetupThread<FD>*)setup)->run();
}

void handler(int signum)
{
    (void)signum;
}

void* handle_interrupt(void* spdz2)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigaction(SIGINT, &action, 0);
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, 0);
    pause();
    ((Spdz2*)spdz2)->stop();
    return 0;
}

int main(int argc, const char** argv)
{
    ez::ezOptionParser opt;
    opt.add(
            "2", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Number of parties (default: 2).", // Help description.
            "-N", // Flag token.
            "--nparties" // Flag token.
    );
    opt.add(
            "", // Default.
            1, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "This player's number, starting with 0 (required).", // Help description.
            "-p", // Flag token.
            "--player" // Flag token.
    );
    opt.add(
            "localhost", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Host where party 0 is running (default: localhost)", // Help description.
            "-h", // Flag token.
            "--hostname" // Flag token.
    );
    opt.add(
            "5000", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Base port number (default: 5000).", // Help description.
            "-pn", // Flag token.
            "--portnum" // Flag token.
    );
    opt.add(
            "128", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Bit length of prime modulus (default: 128)", // Help description.
            "-f", // Flag token.
            "--field-size" // Flag token.
    );
    opt.add(
            "10", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Covert security parameter (default: 10)", // Help description.
            "-c", // Flag token.
            "--covert" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Minimal production with minimal memory (default: run forever)", // Help description.
            "-m", // Flag token.
            "--minimal" // Flag token.
    );
    opt.parse(argc, argv);
    if (!opt.isSet("-p"))
    {
        string usage;
        opt.getUsage(usage);
        cout << usage;
        exit(0);
    }

    int my_num, nplayers;
    int portnum_base;
    string hostname;
    Spdz2 spdz2;
    opt.get("-p")->getInt(my_num);
    opt.get("-N")->getInt(nplayers);
    // 素数长度
    opt.get("-f")->getInt(spdz2.prime_length);
    opt.get("-pn")->getInt(portnum_base);
    opt.get("-h")->getString(hostname);
    opt.get("-c")->getInt(spdz2.covert);
    spdz2.minimal = opt.get("-m")->isSet;

    if(mkdir_p(PREP_DIR) == -1)
    {
        throw runtime_error(
                (string) "cannot use " + PREP_DIR
                        + " (set another PREP_DIR in CONFIG if needed)");
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_BLOCK, &sigset, 0);
    pthread_t interrupt_thread;
    pthread_create(&interrupt_thread, 0, handle_interrupt, &spdz2);

    // 启动一个本地服务
    Server* server = Server::start_networking(spdz2.N, my_num, nplayers, hostname, portnum_base);

    //

    Spdz2SetupThread<FFT_Data> thread_p(spdz2.prime_length, spdz2);
    Spdz2SetupThread<P2Data> thread_2(spdz2.gf2n_length, spdz2);
    pthread_t threads[2];
    if (spdz2.minimal)
    {
        thread_p.signal.unlock();
        thread_2.signal.unlock();
        thread_p.run();
        thread_2.run();
    }
    else
    {
        pthread_create(&threads[0], 0, run_setup<FFT_Data>, &thread_p);
        pthread_create(&threads[1], 0, run_setup<P2Data>, &thread_2);
    }

    // gfp parameter generation is much faster
    if (not spdz2.minimal)
        thread_p.signal.wait();
    thread_p.setup.output(spdz2.N);

    // gf2n is slower
    if (not spdz2.minimal)
        thread_2.signal.wait();
    thread_2.setup.output(spdz2.N);

    for (int i = 0; i < 2; i++)
        if (not spdz2.minimal)
            pthread_join(threads[i], 0);
    pthread_cancel(interrupt_thread);

    if (server)
      delete server;
}
