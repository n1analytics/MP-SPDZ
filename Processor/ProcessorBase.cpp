/*
 * ProcessorBase.cpp
 *
 */

#include "ProcessorBase.hpp"

ProcessorBase::ProcessorBase() :
        input_counter(0), arg(0)
{
}

string ProcessorBase::get_parameterized_filename(int my_num, int thread_num, const string& prefix)
{
    string filename = prefix + "-P" + to_string(my_num) + "-" + to_string(thread_num);
    return filename;
}

void ProcessorBase::open_input_file(int my_num, int thread_num,
        const string& prefix, bool use_dots)
{
    if (use_dots) {
        open_dots_file(thread_num);
    } else {
        string tmp = prefix;
        if (prefix.empty())
            tmp = "Player-Data/Input";

        open_input_file(get_parameterized_filename(my_num, thread_num, tmp));
    }
}

void ProcessorBase::setup_redirection(int my_num, int thread_num,
        OnlineOptions& opts, bool use_dots, SwitchableOutput& out)
{
    if (use_dots) {
        if (thread_num < 0 || (size_t) thread_num >= dots_out_fds_len) {
            throw runtime_error("No DoTS output present for thread num = " + to_string(thread_num));
        }
        output_fdbuf = make_unique<__gnu_cxx::stdio_filebuf<char>>(dots_out_fds[thread_num], ios::out);
        output_file = make_unique<ostream>(output_fdbuf.get());
        out.activate(true);
        out.redirect_to_file(*output_file);
    } else {
        // only output on party 0 if not interactive
        bool always_stdout = opts.cmd_private_output_file == ".";
        bool output = my_num == 0 or opts.interactive or always_stdout;
        out.activate(output);

        if (not (opts.cmd_private_output_file.empty() or always_stdout)) {
            const string stdout_filename = get_parameterized_filename(my_num,
                    thread_num, opts.cmd_private_output_file);
            output_file = make_unique<ofstream>(stdout_filename.c_str());
            out.redirect_to_file(*output_file);
        }
    }
}
