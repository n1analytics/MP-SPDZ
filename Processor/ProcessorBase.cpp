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
        const string& prefix)
{
    string tmp = prefix;
    if (prefix.empty())
        tmp = PREP_DIR "Input";

    open_input_file(get_parameterized_filename(my_num, thread_num, tmp));
}

void ProcessorBase::setup_redirection(int my_num, int thread_num,
		OnlineOptions& opts, SwitchableOutput& out)
{
    // only output on party 0 if not interactive
    bool always_stdout = opts.cmd_private_output_file == ".";
    bool output = my_num == 0 or opts.interactive or always_stdout;
    out.activate(output);

    if (not (opts.cmd_private_output_file.empty() or always_stdout))
    {
        const string stdout_filename = get_parameterized_filename(my_num,
                thread_num, opts.cmd_private_output_file);
        stdout_redirect_file.open(stdout_filename.c_str(), ios_base::out);
        out.redirect_to_file(stdout_redirect_file);
    }
}
