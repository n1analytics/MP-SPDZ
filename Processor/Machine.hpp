#ifndef PROCESSOR_MACHINE_HPP_
#define PROCESSOR_MACHINE_HPP_

#include "Machine.h"

#include "Memory.hpp"
#include "Online-Thread.hpp"
#include "Networking/DotsPlayer.h"
#include "Protocols/Hemi.hpp"
#include "Protocols/fake-stuff.hpp"

#include "Tools/Exceptions.h"

#include <sys/time.h>

#include "Math/Setup.h"
#include "Tools/mkpath.h"
#include "Tools/Bundle.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <pthread.h>
using namespace std;

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::init_binary_domains(int security_parameter, int lg2)
{
  sgf2n::clear::init_field(lg2);

  if (not is_same<typename sgf2n::mac_key_type, GC::NoValue>())
    {
      if (sgf2n::mac_key_type::length() < security_parameter)
        {
          cerr << "Security parameter needs to be at most n in GF(2^n)."
              << endl;
          cerr << "Increase the latter (-lg2) or decrease the former (-S)."
              << endl;
          exit(1);
        }
    }

  if (not is_same<typename sint::bit_type::mac_key_type, GC::NoValue>())
    {
      sint::bit_type::mac_key_type::init_minimum(security_parameter);
    }
  else
    {
      // Initialize field for CCD
      sint::bit_type::part_type::open_type::init_field();
    }
}

template<class sint, class sgf2n>
Machine<sint, sgf2n>::Machine(Names& playerNames, bool use_encryption,
        vector<int> dotsSockets, const OnlineOptions opts, int lg2)
  : my_number(playerNames.my_num()), N(playerNames),
    direct(opts.direct), opening_sum(opts.opening_sum),
    receive_threads(opts.receive_threads), max_broadcast(opts.max_broadcast),
    use_encryption(use_encryption), live_prep(opts.live_prep), opts(opts)
{
  OnlineOptions::singleton = opts;

  if (opening_sum < 2)
    this->opening_sum = N.num_players();
  if (max_broadcast < 2)
    this->max_broadcast = N.num_players();

  // Set up the fields
  sint::clear::read_or_generate_setup(prep_dir_prefix<sint>(), opts);

  init_binary_domains(opts.security_parameter, lg2);

  // make directory for outputs if necessary
  mkdir_p(PREP_DIR);

  string id = "machine";
  if (dotsSockets.size() == 0) {
      if (use_encryption)
        P = new CryptoPlayer(N, id);
      else
        P = new PlainPlayer(N, id);
  } else {
      P = new DotsPlayer(N, id, dotsSockets);
  }

  if (opts.live_prep)
    {
      sint::LivePrep::basic_setup(*P);
    }

  sint::MAC_Check::setup(*P);
  sint::bit_type::MAC_Check::setup(*P);
  sgf2n::MAC_Check::setup(*P);

  alphapi = read_generate_write_mac_key<sint>(*P);
  alpha2i = read_generate_write_mac_key<sgf2n>(*P);
  alphabi = read_generate_write_mac_key<typename
      sint::bit_type::part_type>(*P);

#ifdef DEBUG_MAC
  cerr << "MAC Key p = " << alphapi << endl;
  cerr << "MAC Key 2 = " << alpha2i << endl;
#endif

  // for OT-based preprocessing
  sint::clear::next::template init<typename sint::clear>(false);

  // Initialize the global memory
  auto memtype = opts.memtype;
  if (memtype.compare("old")==0)
     {
       ifstream inpf;
       inpf.open(memory_filename(), ios::in | ios::binary);
       if (inpf.fail()) { throw file_error(memory_filename()); }
       inpf >> M2 >> Mp >> Mi;
       if (inpf.get() != 'M')
         {
           cerr << "Invalid memory file. Run with '-m empty'." << endl;
           exit(1);
         }
       inpf.close();
     }
  else if (!(memtype.compare("empty")==0))
     { cerr << "Invalid memory argument" << endl;
       exit(1);
     }
}

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::prepare(const string& progname_str)
{
  int old_n_threads = nthreads;
  progs.clear();
  load_schedule(progname_str);

  // keep preprocessing
  nthreads = max(old_n_threads, nthreads);

  // initialize persistence if necessary
  for (auto& prog : progs)
    {
      if (prog.writes_persistence)
        {
          string filename = Binary_File_IO::filename(my_number);
          ifstream pers(filename);
          try
          {
              check_file_signature<sint>(pers, filename);
          }
          catch (signature_mismatch&)
          {
              ofstream pers(filename, ios::binary);
              file_signature<sint>().output(pers);
          }
          break;
        }
    }

#ifdef VERBOSE
  progs[0].print_offline_cost();
#endif

  /* Set up the threads */
  tinfo.resize(nthreads);
  threads.resize(nthreads);
  queues.resize(nthreads);
  join_timer.resize(nthreads);

  for (int i = old_n_threads; i < nthreads; i++)
    {
      queues[i] = new ThreadQueue;
      // stand-in for initialization
      queues[i]->schedule({});
      tinfo[i].thread_num=i;
      tinfo[i].Nms=&N;
      tinfo[i].alphapi=&alphapi;
      tinfo[i].alpha2i=&alpha2i;
      tinfo[i].machine=this;
      pthread_create(&threads[i],NULL,thread_info<sint, sgf2n>::Main_Func,&tinfo[i]);
    }

  // synchronize with clients before starting timer
  for (int i=old_n_threads; i<nthreads; i++)
    {
      queues[i]->result();
    }
}

template<class sint, class sgf2n>
Machine<sint, sgf2n>::~Machine()
{
  sint::LivePrep::teardown();
  sgf2n::LivePrep::teardown();

  sint::MAC_Check::teardown();
  sint::bit_type::MAC_Check::teardown();
  sgf2n::MAC_Check::teardown();

  delete P;
  for (auto& queue : queues)
    delete queue;
}

template<class sint, class sgf2n>
size_t Machine<sint, sgf2n>::load_program(const string& threadname,
    const string& filename)
{
  progs.push_back(N.num_players());
  int i = progs.size() - 1;
  progs[i].parse(filename);
  M2.minimum_size(SGF2N, CGF2N, progs[i], threadname);
  Mp.minimum_size(SINT, CINT, progs[i], threadname);
  Mi.minimum_size(NONE, INT, progs[i], threadname);
  return progs.back().size();
}

template<class sint, class sgf2n>
DataPositions Machine<sint, sgf2n>::run_tapes(const vector<int>& args,
    Data_Files<sint, sgf2n>& DataF)
{
  assert(args.size() % 3 == 0);
  for (unsigned i = 0; i < args.size(); i += 3)
    fill_buffers(args[i], args[i + 1], &DataF.DataFp, &DataF.DataFb);
  DataPositions res(N.num_players());
  for (unsigned i = 0; i < args.size(); i += 3)
    res.increase(
        run_tape(args[i], args[i + 1], args[i + 2], DataF.tellg() + res));
  DataF.skip(res);
  return res;
}

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::fill_buffers(int thread_number, int tape_number,
    Preprocessing<sint>* prep,
    Preprocessing<typename sint::bit_type>* bit_prep)
{
  // central preprocessing
  auto usage = progs[tape_number].get_offline_data_used();
  if (sint::expensive and prep != 0 and OnlineOptions::singleton.bucket_size == 3)
    {
      try
      {
          auto& source = *dynamic_cast<BufferPrep<sint>*>(prep);
          auto& dest =
              dynamic_cast<BufferPrep<sint>&>(tinfo[thread_number].processor->DataF.DataFp);
          for (auto it = usage.edabits.begin(); it != usage.edabits.end(); it++)
            {
              bool strict = it->first.first;
              int n_bits = it->first.second;
              size_t required = DIV_CEIL(it->second,
                  sint::bit_type::part_type::default_length);
              auto& dest_buffer = dest.edabits[it->first];
              auto& source_buffer = source.edabits[it->first];
              while (dest_buffer.size() < required)
                {
                  if (source_buffer.empty())
                    source.buffer_edabits(strict, n_bits, &queues);
                  size_t n = min(source_buffer.size(),
                      required - dest_buffer.size());
                  dest_buffer.insert(dest_buffer.end(), source_buffer.end() - n,
                      source_buffer.end());
                  source_buffer.erase(source_buffer.end() - n,
                      source_buffer.end());
                }
            }
      }
      catch (bad_cast& e)
      {
#ifdef VERBOSE_CENTRAL
        cerr << "Problem with central preprocessing" << endl;
#endif
      }
    }

  typedef typename sint::bit_type bit_type;
  if (bit_type::expensive_triples and bit_prep and OnlineOptions::singleton.bucket_size == 3)
    {
      try
      {
          auto& source = *dynamic_cast<BufferPrep<bit_type>*>(bit_prep);
          auto &dest =
              dynamic_cast<BufferPrep<bit_type>&>(tinfo[thread_number].processor->share_thread.DataF);
          for (int i = 0; i < DIV_CEIL(usage.files[DATA_GF2][DATA_TRIPLE],
                                        bit_type::default_length); i++)
            dest.push_triple(source.get_triple_no_count(bit_type::default_length));
      }
      catch (bad_cast& e)
      {
#ifdef VERBOSE_CENTRAL
        cerr << "Problem with central bit triple preprocessing: " << e.what() << endl;
#endif
      }
    }

  if (not HemiOptions::singleton.plain_matmul)
    fill_matmul(thread_number, tape_number, prep, sint::triple_matmul);
}

template<class sint, class sgf2n>
template<int>
void Machine<sint, sgf2n>::fill_matmul(int thread_number, int tape_number,
    Preprocessing<sint>* prep, true_type)
{
  auto usage = progs[tape_number].get_offline_data_used();
  for (auto it = usage.matmuls.begin(); it != usage.matmuls.end(); it++)
    {
      try
      {
          auto& source_proc = *dynamic_cast<BufferPrep<sint>&>(*prep).proc;
          int max_inner = opts.batch_size;
          int max_cols = opts.batch_size;
          for (int j = 0; j < it->first[1]; j += max_inner)
            {
              for (int k = 0; k < it->first[2]; k += max_cols)
                {
                  auto subdim = it->first;
                  subdim[1] = min(subdim[1] - j, max_inner);
                  subdim[2] = min(subdim[2] - k, max_cols);
                  auto& source =
                      dynamic_cast<Hemi<sint>&>(source_proc.protocol).get_matrix_prep(
                          subdim, source_proc);
                  auto& dest =
                      dynamic_cast<Hemi<sint>&>(tinfo[thread_number].processor->Procp.protocol).get_matrix_prep(
                          subdim, tinfo[thread_number].processor->Procp);
                  for (int i = 0; i < it->second; i++)
                    dest.push_triple(source.get_triple_no_count(-1));
                }
            }
      }
      catch (bad_cast& e)
      {
#ifdef VERBOSE_CENTRAL
        cerr << "Problem with central matmul preprocessing: " << e.what() << endl;
#endif
      }
    }
}

template<class sint, class sgf2n>
DataPositions Machine<sint, sgf2n>::run_tape(int thread_number, int tape_number,
    int arg, const DataPositions& pos)
{
  if (size_t(thread_number) >= tinfo.size())
    throw overflow("invalid thread number", thread_number, tinfo.size());
  if (size_t(tape_number) >= progs.size())
    throw overflow("invalid tape number", tape_number, progs.size());

  queues[thread_number]->schedule({tape_number, arg, pos});
  //printf("Send signal to run program %d in thread %d\n",tape_number,thread_number);
  //printf("Running line %d\n",exec);
  if (progs[tape_number].usage_unknown())
    {
      if (not opts.live_prep and thread_number != 0)
        {
          insecure(
              "Internally called tape " + to_string(tape_number)
                  + " has unknown offline data usage");
        }
      return DataPositions(N.num_players());
    }
  else
    {
      // Bits, Triples, Squares, and Inverses skipping
      return progs[tape_number].get_offline_data_used();
    }
}

template<class sint, class sgf2n>
DataPositions Machine<sint, sgf2n>::join_tape(int i)
{
  join_timer[i].start();
  //printf("Waiting for client to terminate\n");
  auto pos = queues[i]->result().pos;
  join_timer[i].stop();
  return pos;
}

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::run_step(const string& progname)
{
  prepare(progname);
  run_tape(0, 0, 0, N.num_players());
  join_tape(0);
}

template<class sint, class sgf2n>
pair<DataPositions, NamedCommStats> Machine<sint, sgf2n>::stop_threads()
{
  // Tell all C-threads to stop
  for (int i=0; i<nthreads; i++)
    {
      //printf("Send kill signal to client\n");
      queues[i]->schedule(-1);
    }

  // sum actual usage
  DataPositions pos(N.num_players());

#ifdef DEBUG_THREADS
  cerr << "Waiting for all clients to finish" << endl;
#endif
  // Wait until all clients have signed out
  for (int i=0; i<nthreads; i++)
    {
      queues[i]->schedule({});
      pos.increase(queues[i]->result().pos);
      pthread_join(threads[i],NULL);
    }

  auto comm_stats = total_comm();

  for (auto& queue : queues)
    delete queue;

  queues.clear();

  nthreads = 0;

  return {pos, comm_stats};
}

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::run(const string& progname)
{
  prepare(progname);

  Timer proc_timer(CLOCK_PROCESS_CPUTIME_ID);
  proc_timer.start();
  timer[0].start({});

  // run main tape
  run_tape(0, 0, 0, N.num_players());
  join_tape(0);

  print_compiler();

  finish_timer.start();

  // actual usage
  auto res = stop_threads();
  DataPositions& pos = res.first;

  finish_timer.stop();
  
#ifdef VERBOSE
  cerr << "Memory usage: ";
  tinfo[0].print_usage(cerr, Mp.MS, "sint");
  tinfo[0].print_usage(cerr, Mp.MC, "cint");
  tinfo[0].print_usage(cerr, M2.MS, "sgf2n");
  tinfo[0].print_usage(cerr, M2.MS, "cgf2n");
  tinfo[0].print_usage(cerr, bit_memories.MS, "sbits");
  tinfo[0].print_usage(cerr, bit_memories.MC, "cbits");
  tinfo[0].print_usage(cerr, Mi.MC, "regint");
  cerr << endl;

  for (unsigned int i = 0; i < join_timer.size(); i++)
    cerr << "Join timer: " << i << " " << join_timer[i].elapsed() << endl;
  cerr << "Finish timer: " << finish_timer.elapsed() << endl;
#endif

  NamedCommStats& comm_stats = res.second;

  if (opts.verbose)
    {
      cerr << "Communication details "
          "(rounds in parallel threads counted double):" << endl;
      comm_stats.print();
      cerr << "CPU time = " <<  proc_timer.elapsed() << endl;
    }

  print_timers();

  size_t rounds = 0;
  for (auto& x : comm_stats)
      rounds += x.second.rounds;
  cerr << "Data sent = " << comm_stats.sent / 1e6 << " MB in ~" << rounds
      << " rounds (party " << my_number;
  if (threads.size() > 1)
      cerr << "; rounds counted double due to multi-threading";
  cerr << ")" << endl;

  auto& P = *this->P;
  this->print_global_comm(P, comm_stats);

#ifdef VERBOSE_OPTIONS
  if (opening_sum < N.num_players() && !direct)
    cerr << "Summed at most " << opening_sum << " shares at once with indirect communication" << endl;
  else
    cerr << "Summed all shares at once" << endl;

  if (max_broadcast < N.num_players() && !direct)
    cerr << "Send to at most " << max_broadcast << " parties at once" << endl;
  else
    cerr << "Full broadcast" << endl;
#endif

#ifdef CHOP_MEMORY
  // Reduce memory size to speed up
  unsigned max_size = 1 << 20;
  if (M2.size_s() > max_size)
    M2.resize_s(max_size);
  if (Mp.size_s() > max_size)
    Mp.resize_s(max_size);
#endif

  // Write out the memory to use next time
  ofstream outf(memory_filename(), ios::out | ios::binary);
  outf << M2 << Mp << Mi;
  outf << 'M';
  outf.close();

  bit_memories.write_memory(N.my_num());

#ifdef OLD_USAGE
  for (int dtype = 0; dtype < N_DTYPE; dtype++)
    {
      cerr << "Num " << DataPositions::dtype_names[dtype] << "\t=";
      for (int field_type = 0; field_type < N_DATA_FIELD_TYPE; field_type++)
        cerr << " " << pos.files[field_type][dtype];
      cerr << endl;
   }
  for (int field_type = 0; field_type < N_DATA_FIELD_TYPE; field_type++)
    {
      cerr << "Num " << DataPositions::field_names[field_type] << " Inputs\t=";
      for (int i = 0; i < N.num_players(); i++)
        cerr << " " << pos.inputs[i][field_type];
      cerr << endl;
    }
#endif

  if (opts.verbose)
    {
      cerr << "Actual cost of program:" << endl;
      pos.print_cost();
    }

  if (pos.any_more(progs[0].get_offline_data_used())
      and not progs[0].usage_unknown())
    throw runtime_error("computation used more preprocessing than expected");

  if (not stats.empty())
    {
      stats.print();
    }

  if (not opts.file_prep_per_thread)
    {
      Data_Files<sint, sgf2n> df(*this);
      df.seekg(pos);
      df.prune();
    }

  suggest_optimizations();

#ifdef VERBOSE
  cerr << "End of prog" << endl;
#endif
}

template<class sint, class sgf2n>
string Machine<sint, sgf2n>::memory_filename()
{
  return BaseMachine::memory_filename(sint::type_short(), my_number);
}

template<class sint, class sgf2n>
template<class T>
string Machine<sint, sgf2n>::prep_dir_prefix()
{
  return opts.prep_dir_prefix<T>(N.num_players());
}

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::reqbl(int n)
{
  sint::clear::reqbl(n);
}

template<class sint, class sgf2n>
void Machine<sint, sgf2n>::suggest_optimizations()
{
  string optimizations;
  if (relevant_opts.find("trunc_pr") != string::npos and sint::has_trunc_pr)
    optimizations.append("\tprogram.use_trunc_pr = True\n");
  if (relevant_opts.find("split") != string::npos and sint::has_split)
    optimizations.append(
        "\tprogram.use_split(" + to_string(N.num_players()) + ")\n");
  if (relevant_opts.find("edabit") != string::npos and not sint::has_split)
    optimizations.append("\tprogram.use_edabit(True)\n");
  if (not optimizations.empty())
    cerr << "This program might benefit from some protocol options." << endl
        << "Consider adding the following at the beginning of '" << progname
        << ".mpc':" << endl << optimizations;
}

#endif
