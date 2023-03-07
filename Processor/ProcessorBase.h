/*
 * ProcessorBase.h
 *
 */

#ifndef PROCESSOR_PROCESSORBASE_H_
#define PROCESSOR_PROCESSORBASE_H_

#include <stack>
#include <string>
#include <fstream>
#include <memory>
#include <ext/stdio_filebuf.h>
using namespace std;

#include "Tools/ExecutionStats.h"
#include "Tools/SwitchableOutput.h"
#include "OnlineOptions.h"

class ProcessorBase
{
  // Stack
  stack<long> stacki;

  unique_ptr<istream> input_file;
  unique_ptr<__gnu_cxx::stdio_filebuf<char>> input_fdbuf;
  string input_filename;
  size_t input_counter;

protected:
  // Optional argument to tape
  int arg;

  string get_parameterized_filename(int my_num, int thread_num,
      const string& prefix);

public:
  ExecutionStats stats;

  unique_ptr<ostream> output_file;
  unique_ptr<__gnu_cxx::stdio_filebuf<char>> output_fdbuf;

  ProcessorBase();

  void pushi(long x) { stacki.push(x); }
  void popi(long& x) { x = stacki.top(); stacki.pop(); }

  int get_arg() const
    {
      return arg;
    }

  void set_arg(int new_arg)
    {
      arg=new_arg;
    }

  void open_input_file(const string& name);
  void open_dots_file(int thread_num);
  void open_input_file(int my_num, int thread_num, const string& prefix="", bool use_dots = false);

  template<class T>
  T get_input(bool interactive, const int* params);
  template<class T>
  T get_input(istream& is, const string& input_filename, const int* params);

  void setup_redirection(int my_nu, int thread_num, OnlineOptions& opts,
          bool use_dots, SwitchableOutput& out);
};

#endif /* PROCESSOR_PROCESSORBASE_H_ */
