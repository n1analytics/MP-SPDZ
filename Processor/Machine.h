/*
 * Machine.h
 *
 */

#ifndef MACHINE_H_
#define MACHINE_H_

#include "Processor/BaseMachine.h"
#include "Processor/Memory.h"
#include "Processor/Program.h"
#include "Processor/OnlineOptions.h"

#include "Processor/Online-Thread.h"
#include "Processor/ThreadJob.h"

#include "GC/Machine.h"

#include "Tools/time-func.h"
#include "Tools/ExecutionStats.h"

#include <vector>
#include <map>
#include <atomic>
using namespace std;

template<class sint, class sgf2n>
class Machine : public BaseMachine
{
  /* The mutex's lock the C-threads and then only release
   * then we an MPC thread is ready to run on the C-thread.
   * Control is passed back to the main loop when the
   * MPC thread releases the mutex
   */

  vector<thread_info<sint, sgf2n>> tinfo;
  vector<pthread_t> threads;

  int my_number;
  Names& N;
  typename sint::mac_key_type alphapi;
  typename sgf2n::mac_key_type alpha2i;
  typename sint::bit_type::mac_key_type alphabi;

  Player* P;

  size_t load_program(const string& threadname, const string& filename);

  void prepare(const string& progname_str);

  void suggest_optimizations();

  public:

  vector<Program>  progs;

  Memory<sgf2n> M2;
  Memory<sint> Mp;
  Memory<Integer> Mi;
  GC::Memories<typename sint::bit_type> bit_memories;

  vector<Timer> join_timer;
  Timer finish_timer;

  bool direct;
  int opening_sum;
  bool receive_threads;
  int max_broadcast;
  bool use_encryption;
  bool live_prep;

  OnlineOptions opts;

  ExecutionStats stats;

  static void init_binary_domains(int security_parameter, int lg2);

  Machine(Names& playerNames, bool use_encryption = true,
          vector<int> dotsSockets = {}, const OnlineOptions opts = sint(),
          int lg2 = 0);
  ~Machine();

  const Names& get_N() { return N; }

  DataPositions run_tapes(const vector<int> &args,
      Data_Files<sint, sgf2n>& DataF);
  void fill_buffers(int thread_number, int tape_number,
      Preprocessing<sint> *prep,
      Preprocessing<typename sint::bit_type> *bit_prep);
  template<int = 0>
  void fill_matmul(int thread_numbber, int tape_number,
      Preprocessing<sint> *prep, true_type);
  template<int = 0>
  void fill_matmul(int, int, Preprocessing<sint>*, false_type) {}
  DataPositions run_tape(int thread_number, int tape_number, int arg,
      const DataPositions& pos);
  DataPositions join_tape(int thread_number);

  void run(const string& progname);

  void run_step(const string& progname);
  pair<DataPositions, NamedCommStats> stop_threads();

  string memory_filename();

  template<class T>
  string prep_dir_prefix();

  void reqbl(int n);

  typename sint::bit_type::mac_key_type get_bit_mac_key() { return alphabi; }
  typename sint::mac_key_type get_sint_mac_key() { return alphapi; }

  Player& get_player() { return *P; }
};

#endif /* MACHINE_H_ */
