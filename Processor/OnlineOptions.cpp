/*
 * OnlineOptions.cpp
 *
 */

#include "OnlineOptions.h"
#include "BaseMachine.h"
#include "Math/gfp.h"
#include "Math/gfpvar.h"

#include "Math/gfp.hpp"

#include <string>
#include "PpcConstant.h"

using namespace std;

OnlineOptions OnlineOptions::singleton;

OnlineOptions::OnlineOptions() : playerno(-1)
{
    interactive = false;
    lgp = gfp0::MAX_N_BITS;
    live_prep = true;
    batch_size = 10000;
    memtype = "empty";
    bits_from_squares = false;
    direct = false;
    bucket_size = 4;
    cmd_private_input_file = "Player-Data/Input";
    cmd_private_output_file = "";
}

OnlineOptions::OnlineOptions(ez::ezOptionParser& opt, int argc,
        const char** argv, int default_batch_size, bool default_live_prep,
        bool variable_prime_length) :
        OnlineOptions()
{
    if (default_batch_size <= 0)
        default_batch_size = batch_size;

    opt.syntax = std::string(argv[0]) + " [OPTIONS] [<playerno>] <progname>";

    opt.add(
          "", // Default.
          0, // Required?
          0, // Number of args expected.
          0, // Delimiter if expecting multiple args.
          "Interactive mode in the main thread (default: disabled)", // Help description.
          "-I", // Flag token.
          "--interactive" // Flag token.
    );
    opt.add(
          cmd_private_input_file.c_str(), // Default.
          0, // Required?
          1, // Number of args expected.
          0, // Delimiter if expecting multiple args.
          "Prefix for input file path (default: Player-Data/Private-Input). "
          "Input will be read from {prefix}-P{id}-{thread_id}.", // Help description.
          "-IF", // Flag token.
          "--input-file" // Flag token.
    );
    opt.add(
          cmd_private_output_file.c_str(), // Default.
          0, // Required?
          1, // Number of args expected.
          0, // Delimiter if expecting multiple args.
          "Prefix for output file path "
          "(default: output to stdout for party 0 (silent otherwise "
          "unless interactive mode is active). "
          "Output will be written to {prefix}-P{id}-{thread_id}.", // Help description.
          "-OF", // Flag token.
          "--output-file" // Flag token.
    );

    string default_lgp = to_string(lgp);
    if (variable_prime_length)
    {
        opt.add(
                default_lgp.c_str(), // Default.
                0, // Required?
                1, // Number of args expected.
                0, // Delimiter if expecting multiple args.
                ("Bit length of GF(p) field (default: " + default_lgp + ")").c_str(), // Help description.
                "-lgp", // Flag token.
                "--lgp" // Flag token.
        );
        opt.add(
                "", // Default.
                0, // Required?
                1, // Number of args expected.
                0, // Delimiter if expecting multiple args.
                "Prime for GF(p) field (default: read from file or "
                "generated from -lgp argument)", // Help description.
                "-P", // Flag token.
                "--prime" // Flag token.
        );
    }
    if (default_live_prep)
        opt.add(
                "", // Default.
                0, // Required?
                0, // Number of args expected.
                0, // Delimiter if expecting multiple args.
                "Preprocessing from files", // Help description.
                "-F", // Flag token.
                "--file-preprocessing" // Flag token.
        );
    else
        opt.add(
                "", // Default.
                0, // Required?
                0, // Number of args expected.
                0, // Delimiter if expecting multiple args.
                "Live preprocessing", // Help description.
                "-L", // Flag token.
                "--live-preprocessing" // Flag token.
        );
    opt.add(
            "", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "This player's number (required if not given before program name)", // Help description.
            "-p", // Flag token.
            "--player" // Flag token.
    );

    opt.add(
            to_string(default_batch_size).c_str(), // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            ("Size of preprocessing batches (default: " + to_string(default_batch_size) + ")").c_str(), // Help description.
            "-b", // Flag token.
            "--batch-size" // Flag token.
    );
    opt.add(
            memtype.c_str(), // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Where to obtain memory, old|empty (default: empty)\n\t"
            "old: reuse previous memory in Memory-<type>-P<i>\n\t"
            "empty: create new empty memory", // Help description.
            "-m", // Flag token.
            "--memory" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Compute random bits from squares", // Help description.
            "-Q", // Flag token.
            "--bits-from-squares" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            0, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Direct communication instead of star-shaped "
            "(only for dishonest-majority protocols)", // Help description.
            "-d", // Flag token.
            "--direct" // Flag token.
    );
    opt.add(
            "4", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Batch size for sacrifice (3-5, default: 4)", // Help description.
            "-B", // Flag token.
            "--bucket-size" // Flag token.
    );
    opt.add(
            "", // Default.
            0, // Required?
            1, // Number of args expected.
            0, // Delimiter if expecting multiple args.
            "Where to obtain memory file, old|empty (default: empty)\n\t"
            "old: reuse previous memory in Memory-<type>-P<i>\n\t"
            "empty: create new empty memory", // Help description.
            "-MF", // Flag token.
            "--memory-file" // Flag token.
    );
    opt.add("",                      // Default.
        0,                           // Required?
        1,                           // Number of args expected.
        0,                           // Delimiter if expecting multiple args.
        "Client connection waiting time (millisecond)",  // Help description.
        "-CW",                        // Flag token.
        "--connection-waiting"             // Flag token.
    );
    opt.add("",                      // Default.
            0,                           // Required?
            0,                           // Number of args expected.
            0,                           // Delimiter if expecting multiple args.
            "PPC debug model",  // Help description.
            "-DBG",                        // Flag token.
            "--debug"       // Flag token.
        );

    opt.parse(argc, argv);

    interactive = opt.isSet("-I");
    if (variable_prime_length)
    {
        opt.get("--lgp")->getInt(lgp);
        string p;
        opt.get("--prime")->getString(p);
        if (not p.empty())
            prime = bigint(p);
    }
    if (default_live_prep)
        live_prep = not opt.get("-F")->isSet;
    else
        live_prep = opt.get("-L")->isSet;
    opt.get("-b")->getInt(batch_size);
    opt.get("--memory")->getString(memtype);
    bits_from_squares = opt.isSet("-Q");

    opt.get("-IF")->getString(cmd_private_input_file);
    opt.get("-OF")->getString(cmd_private_output_file);

    std::string prefix;
    opt.get("-MF")->getString(prefix);
    set_prefix(prefix);

    int connection_waiting_flag;
    opt.get("-CW")->getInt(connection_waiting_flag);
    set_connection_waiting_millisecond_flag(connection_waiting_flag);
    bool debug_flag = opt.isSet("--debug");
    set_debug_flag(debug_flag);

    direct = opt.isSet("--direct");

    opt.get("--bucket-size")->getInt(bucket_size);

    opt.resetArgs();
}

void OnlineOptions::finalize(ez::ezOptionParser& opt, int argc,
        const char** argv)
{
    opt.resetArgs();
    opt.parse(argc, argv);

    vector<string*> allArgs(opt.firstArgs);
    allArgs.insert(allArgs.end(), opt.unknownArgs.begin(), opt.unknownArgs.end());
    allArgs.insert(allArgs.end(), opt.lastArgs.begin(), opt.lastArgs.end());
    string usage;
    vector<string> badOptions;
    unsigned int i;

    if (allArgs.size() != 3u - opt.isSet("-p"))
    {
        cerr << "ERROR: incorrect number of arguments to " << argv[0] << endl;
        cerr << "Arguments given were:\n";
        for (unsigned int j = 1; j < allArgs.size(); j++)
            cout << "'" << *allArgs[j] << "'" << endl;
        opt.getUsage(usage);
        cout << usage;
        exit(1);
    }
    else
    {
        if (opt.isSet("-p"))
            opt.get("-p")->getInt(playerno);
        else
            sscanf((*allArgs[1]).c_str(), "%d", &playerno);
        progname = *allArgs[2 - opt.isSet("-p")];
    }

    if (!opt.gotRequired(badOptions))
    {
        for (i = 0; i < badOptions.size(); ++i)
            cerr << "ERROR: Missing required option " << badOptions[i] << ".";
        opt.getUsage(usage);
        cout << usage;
        exit(1);
    }

    if (!opt.gotExpected(badOptions))
    {
        for (i = 0; i < badOptions.size(); ++i)
            cerr << "ERROR: Got unexpected number of arguments for option "
                    << badOptions[i] << ".";
        opt.getUsage(usage);
        cout << usage;
        exit(1);
    }

    if (opt.get("-lgp"))
    {
        bigint schedule_prime = BaseMachine::prime_from_schedule(progname);
        if (prime != 0 and prime != schedule_prime and schedule_prime != 0)
        {
            cerr << "Different prime for compilation and computation." << endl;
            cerr << "Run with '--prime " << schedule_prime
                    << "' or compile with '--prime " << prime << "'." << endl;
            exit(1);
        }
        if (schedule_prime != 0)
            prime = schedule_prime;
    }

    if (opt.get("-lgp") and not opt.isSet("-lgp"))
    {
        int prog_lgp = BaseMachine::prime_length_from_schedule(progname);
        prog_lgp = DIV_CEIL(prog_lgp, 64) * 64;
        if (prog_lgp != 0)
            lgp = prog_lgp;

#ifndef FEWER_PRIMES
        if (prime_limbs() > 4)
#endif
            lgp = max(lgp, gfp0::MAX_N_BITS);
    }
}

int OnlineOptions::prime_length()
{
    if (prime == 0)
        return lgp;
    else
        return prime.numBits();
}

int OnlineOptions::prime_limbs()
{
    return DIV_CEIL(prime_length(), 64);
}
