/*
 * ProcessorBase.cpp
 *
 */

#ifndef PROCESSOR_PROCESSORBASE_HPP_
#define PROCESSOR_PROCESSORBASE_HPP_

#include "ProcessorBase.h"
#include "IntInput.h"
#include "FixInput.h"
#include "FloatInput.h"
#include "Tools/Exceptions.h"

#include <memory>
#include <iostream>
#include <ext/stdio_filebuf.h>
#include <dots.h>

inline
void ProcessorBase::open_input_file(const string& name)
{
#ifdef DEBUG_FILES
    cerr << "opening " << name << endl;
#endif

    input_file = std::make_unique<ifstream>(name);
}

inline
void ProcessorBase::open_dots_file(int thread_num)
{
    if (thread_num < 0 || (size_t) thread_num >= dots_in_fds_len) {
        throw runtime_error("No DoTS input present for thread num = " + to_string(thread_num));
    }
    input_fdbuf = make_unique<__gnu_cxx::stdio_filebuf<char>>(dots_in_fds[thread_num], ios::in);
    input_file = make_unique<istream>(input_fdbuf.get());
}

template<class T>
T ProcessorBase::get_input(bool interactive, const int* params)
{
    if (interactive)
        return get_input<T>(cin, "standard input", params);
    else
        return get_input<T>(*input_file, input_filename, params);
}

template<class T>
T ProcessorBase::get_input(istream& input_file, const string& input_filename, const int* params)
{
    T res;
    if (input_file.peek() == EOF)
        throw IO_Error("not enough inputs in " + input_filename);
    res.read(input_file, params);
    if (input_file.fail())
    {
        throw input_error(T::NAME, input_filename, input_file, input_counter);
    }
    input_counter++;
    return res;
}

#endif
