/*
 * BaseMachine.cpp
 *
 */

#include "BaseMachine.h"
#include "Math/Setup.h"
#include "PpcConstant.h"

#include <iostream>
#include <sodium.h>
using namespace std;

BaseMachine* BaseMachine::singleton = 0;
thread_local int BaseMachine::thread_num;

void print_usage(ostream& o, const char* name, size_t capacity)
{
  if (capacity)
    o << name << "=" << capacity << " ";
}

BaseMachine& BaseMachine::s()
{
  if (singleton)
    return *singleton;
  else
    throw runtime_error("no singleton");
}

BaseMachine::BaseMachine() : nthreads(0)
{
  if (sodium_init() == -1)
    throw runtime_error("couldn't initialize libsodium");
  if (not singleton)
    singleton = this;
}

void BaseMachine::load_schedule(const string& progname, bool load_bytecode)
{
  this->progname = progname;
  string fname = "Programs/Schedules/" + progname + ".sch";
#ifdef DEBUG_FILES
  cerr << "Opening file " << fname << endl;
#endif
  ifstream inpf;
  inpf.open(fname);
  if (inpf.fail()) { throw file_error("Missing '" + fname + "'. Did you compile '" + progname + "'?"); }

  int nprogs;
  inpf >> nthreads;
  inpf >> nprogs;

  if (inpf.fail())
    throw file_error("Error reading " + fname);

#ifdef DEBUG_FILES
  cerr << "Number of threads I will run in parallel = " << nthreads << endl;
  cerr << "Number of program sequences I need to load = " << nprogs << endl;
#endif

  // Load in the programs
  string threadname;
  for (int i=0; i<nprogs; i++)
    { inpf >> threadname;
      string filename = "Programs/Bytecode/" + threadname + ".bc";
      bc_filenames.push_back(filename);
      if (load_bytecode)
        {
#ifdef DEBUG_FILES
          cerr << "Loading program " << i << " from " << filename << endl;
#endif
          load_program(threadname, filename);
        }
    }

  for (auto i : {1, 0, 0})
    {
      int n;
      inpf >> n;
      if (n != i)
        throw runtime_error("old schedule format not supported");
    }

  inpf.get();
  getline(inpf, compiler);
  getline(inpf, domain);
  getline(inpf, relevant_opts);
  inpf.close();
}

void BaseMachine::print_compiler()
{
#ifdef VERBOSE
  if (compiler.size() != 0)
    cerr << "Compiler: " << compiler << endl;
#endif
}

void BaseMachine::load_program(const string& threadname, const string& filename)
{
  (void)threadname;
  (void)filename;
  throw not_implemented();
}

void BaseMachine::time()
{
  cout << "Elapsed time: " << timer[0].elapsed() << endl;
}

void BaseMachine::start(int n)
{
  cout << "Starting timer " << n << " at " << timer[n].elapsed()
    << " after " << timer[n].idle() << endl;
  timer[n].start();
}

void BaseMachine::stop(int n)
{
  timer[n].stop();
  cout << "Stopped timer " << n << " at " << timer[n].elapsed() << endl;
}

void BaseMachine::print_timers()
{
  cerr << "Time = " << timer[0].elapsed() << " seconds " << endl;
  timer.erase(0);
  for (map<int,Timer>::iterator it = timer.begin(); it != timer.end(); it++)
    cerr << "Time" << it->first << " = " << it->second.elapsed() << " seconds " << endl;
}

string BaseMachine::memory_filename(const string &type_short, int my_number) {
  std::string prefix = get_prefix();
  if (prefix.size() != 0) {
    return PREP_DIR "" + prefix + "-Memory-" + type_short + "-P" +
           to_string(my_number);
  }
  return PREP_DIR "Memory-" + type_short + "-P" + to_string(my_number);
}

string BaseMachine::get_domain(string progname)
{
  assert(not singleton);
  BaseMachine machine;
  singleton = 0;
  machine.load_schedule(progname, false);
  return machine.domain;
}

int BaseMachine::ring_size_from_schedule(string progname)
{
  string domain = get_domain(progname);
  if (domain.substr(0, 2).compare("R:") == 0)
  {
    return stoi(domain.substr(2));
  }
  else
    return 0;
}

int BaseMachine::prime_length_from_schedule(string progname)
{
  string domain = get_domain(progname);
  if (domain.substr(0, 4).compare("lgp:") == 0)
    return stoi(domain.substr(4));
  else
    return 0;
}

bigint BaseMachine::prime_from_schedule(string progname)
{
  string domain = get_domain(progname);
  if (domain.substr(0, 2).compare("p:") == 0)
    return bigint(domain.substr(2));
  else
    return 0;
}
