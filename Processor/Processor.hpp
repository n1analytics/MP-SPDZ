#ifndef PROCESSOR_PROCESSOR_HPP_
#define PROCESSOR_PROCESSOR_HPP_

#include "Processor/Processor.h"
#include "Processor/Program.h"
#include "GC/square64.h"
#include "SpecificPrivateOutput.h"

#include "Processor/ProcessorBase.hpp"
#include "GC/Processor.hpp"
#include "GC/ShareThread.hpp"
#include "Protocols/SecureShuffle.hpp"

#include <sodium.h>
#include <string>

template <class T>
SubProcessor<T>::SubProcessor(ArithmeticProcessor& Proc, typename T::MAC_Check& MC,
    Preprocessing<T>& DataF, Player& P) :
    SubProcessor<T>(MC, DataF, P, &Proc)
{
}

template <class T>
SubProcessor<T>::SubProcessor(typename T::MAC_Check& MC,
    Preprocessing<T>& DataF, Player& P, ArithmeticProcessor* Proc) :
    shuffler(*this),
    Proc(Proc), MC(MC), P(P), DataF(DataF), protocol(P), input(*this, MC),
    bit_prep(bit_usage)
{
  DataF.set_proc(this);
  protocol.init(DataF, MC);
  DataF.set_protocol(protocol);
  bit_usage.set_num_players(P.num_players());
  personal_bit_preps.resize(P.num_players());
  for (int i = 0; i < P.num_players(); i++)
    personal_bit_preps[i] = new typename BT::LivePrep(bit_usage, i);
}

template<class T>
SubProcessor<T>::~SubProcessor()
{
  for (size_t i = 0; i < personal_bit_preps.size(); i++)
    {
      auto& x = personal_bit_preps[i];
      delete x;
    }
#ifdef VERBOSE
  if (not bit_usage.empty())
    {
      cerr << "Mixed-circuit preprocessing cost:" << endl;
      bit_usage.print_cost();
    }
#endif
}

template<class sint, class sgf2n>
Processor<sint, sgf2n>::Processor(int thread_num,Player& P,
        typename sgf2n::MAC_Check& MC2,typename sint::MAC_Check& MCp,
        Machine<sint, sgf2n>& machine,
        const Program& program)
: ArithmeticProcessor(machine.opts, thread_num),DataF(machine, &Procp, &Proc2),P(P),
  MC2(MC2),MCp(MCp),machine(machine),
  share_thread(DataF.DataFb, P, machine.get_bit_mac_key()),
  Procb(machine.bit_memories),
  Proc2(*this,MC2,DataF.DataF2,P),Procp(*this,MCp,DataF.DataFp,P),
  external_clients(P.my_num()),
  binary_file_io(Binary_File_IO())
{
  reset(program,0);

  public_input_filename = get_filename("Programs/Public-Input/",false);
  public_input.open(public_input_filename);
  private_input_filename = (get_filename(PREP_DIR "Private-Input-",true));
  private_input.open(private_input_filename.c_str());
  public_output.open(get_filename(PREP_DIR "Public-Output-",true).c_str(), ios_base::out);
  binary_output.open(
      get_parameterized_filename(P.my_num(), thread_num,
          PREP_DIR "Binary-Output"), ios_base::out);

  open_input_file(P.my_num(), thread_num, machine.opts.cmd_private_input_file, machine.use_dots);

  secure_prng.ReSeed();
  shared_prng.SeedGlobally(P, false);

  setup_redirection(P.my_num(), thread_num, opts, machine.use_dots, out);
  Procb.out = out;
}


template<class sint, class sgf2n>
Processor<sint, sgf2n>::~Processor()
{
  share_thread.post_run();
#ifdef VERBOSE
  if (sent)
    cerr << "Opened " << sent << " elements in " << rounds << " rounds" << endl;
#endif
}

template<class sint, class sgf2n>
string Processor<sint, sgf2n>::get_filename(const char* prefix, bool use_number)
{
  stringstream filename;
  filename << prefix;
  if (!use_number)
    filename << machine.progname;
  if (use_number)
    filename << P.my_num();
  if (thread_num > 0)
    filename << "-" << thread_num;
#ifdef DEBUG_FILES
  cerr << "Opening file " << filename.str() << endl;
#endif
  return filename.str();
}


template<class sint, class sgf2n>
void Processor<sint, sgf2n>::reset(const Program& program,int arg)
{
  Proc2.get_S().resize(program.num_reg(SGF2N));
  Proc2.get_C().resize(program.num_reg(CGF2N));
  Procp.get_S().resize(program.num_reg(SINT));
  Procp.get_C().resize(program.num_reg(CINT));
  Ci.resize(program.num_reg(INT));
  this->arg = arg;
  Procb.reset(program);
}

template<class T>
void SubProcessor<T>::check()
{
  // protocol check before last MAC check
  protocol.check();
  // MACCheck
  MC.Check(P);
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::check()
{
  Procp.check();
  Proc2.check();
  share_thread.check();

  //cout << num << " : Checking broadcast" << endl;
  P.Check_Broadcast();
  //cout << num << " : Broadcast checked "<< endl;
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::dabit(const Instruction& instruction)
{
  int size = instruction.get_size();
  int unit = sint::bit_type::default_length;
  for (int i = 0; i < DIV_CEIL(size, unit); i++)
  {
    Procb.S[instruction.get_r(1) + i] = {};
  }
  for (int i = 0; i < size; i++)
  {
    typename sint::bit_type tmp;
    Procp.DataF.get_dabit(Procp.get_S_ref(instruction.get_r(0) + i), tmp);
    Procb.S[instruction.get_r(1) + i / unit] ^= tmp << (i % unit);
  }
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::edabit(const Instruction& instruction, bool strict)
{
  auto& regs = instruction.get_start();
  int size = instruction.get_size();
  Procp.DataF.get_edabits(strict, size,
          &Procp.get_S_ref(instruction.get_r(0)), Procb.S, regs);
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::convcbitvec(const Instruction& instruction)
{
  for (size_t i = 0; i < instruction.get_n(); i++)
    {
      int i1 = i / GC::Clear::N_BITS;
      int i2 = i % GC::Clear::N_BITS;
      Ci[instruction.get_r(0) + i] = Procb.C[instruction.get_r(1) + i1].get_bit(i2);
    }
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::convcintvec(const Instruction& instruction)
{
  int unit = GC::Clear::N_BITS;
  assert(unit == 64);
  int n_inputs = instruction.get_size();
  int n_bits = instruction.get_start().size();
  for (int i = 0; i < DIV_CEIL(n_inputs, unit); i++)
    {
      for (int j = 0; j < DIV_CEIL(n_bits, unit); j++)
        {
          square64 square;
          int n_rows = min(n_inputs - i * unit, unit);
          int n_cols = min(n_bits - j * unit, unit);
          for (int k = 0; k < n_rows; k++)
            square.rows[k] =
                Integer::convert_unsigned(
                    Procp.C[instruction.get_r(0) + i * unit + k] >> (j * unit)).get();
          square.transpose(n_rows, n_cols);
          for (int k = 0; k < n_cols; k++)
            Procb.C[instruction.get_start()[k + j * unit] + i] = square.rows[k];
        }
    }
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::split(const Instruction& instruction)
{
  int n = instruction.get_n();
  assert (instruction.get_start().size() % n == 0);
  int unit = GC::Clear::N_BITS;
  assert(unit == 64);
  int n_inputs = instruction.get_size();
  int n_bits = instruction.get_start().size() / n;
  assert(share_thread.protocol != 0);
  sint::split(Procb.S, instruction.get_start(), n_bits,
      &read_Sp(instruction.get_r(0)), n_inputs, *share_thread.protocol);
}


#include "Networking/sockets.h"
#include "Math/Setup.h"

// Write socket (typically SPDZ engine -> external client), for different register types.
// RegType and SecrecyType determines how registers are read and the socket stream is packed.
// If message_type is > 0, send message_type in bytes 0 - 3, to allow an external client to
//  determine the data structure being sent in a message.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::write_socket(const RegType reg_type,
    bool send_macs, int socket_id, int message_type,
    const vector<int>& registers, int size)
{
  int m = registers.size();
  socket_stream.reset_write_head();

  //First 4 bytes is message_type (unless indicate not needed)
  if (message_type != 0) {
    socket_stream.store(message_type);
  }

  for (int j = 0; j < size; j++)
    {
      for (int i = 0; i < m; i++)
        {
          if (reg_type == SINT)
            {
              // Send vector of secret shares and optionally macs
              if (send_macs)
                get_Sp_ref(registers[i] + j).pack(socket_stream);
              else
                get_Sp_ref(registers[i] + j).pack(socket_stream,
                    sint::get_rec_factor(P.my_num(), P.num_players()));
            }
          else if (reg_type == CINT)
            {
              // Send vector of clear public field elements
              get_Cp_ref(registers[i] + j).pack(socket_stream);
            }
          else if (reg_type == INT)
            {
              // Send vector of 32-bit clear ints
              socket_stream.store((int&) get_Ci_ref(registers[i] + j));
            }
          else
            {
              stringstream ss;
              ss << "Write socket instruction with unknown reg type "
                  << reg_type << "." << endl;
              throw Processor_Error(ss.str());
            }
        }
    }

#ifdef VERBOSE_COMM
  cerr << "send " << socket_stream.get_length() << " to client " << socket_id
       << endl;
#endif

  try {
    socket_stream.Send(external_clients.get_socket(socket_id));
  }
    catch (bad_value& e) {
    cerr << "Send error thrown when writing " << m << " values of type " << reg_type << " to socket id " 
      << socket_id << "." << endl;
  }
}


// Receive vector of 32-bit clear ints
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_socket_ints(int client_id,
    const vector<int>& registers, int size)
{
  int m = registers.size();
  socket_stream.reset_write_head();
  socket_stream.Receive(external_clients.get_socket(client_id));
  for (int j = 0; j < size; j++)
    for (int i = 0; i < m; i++)
      {
        int val;
        socket_stream.get(val);
        write_Ci(registers[i] + j, (long) val);
      }
}

// Receive vector of public field elements
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_socket_vector(int client_id,
    const vector<int>& registers, int size)
{
  int m = registers.size();
  socket_stream.reset_write_head();
  socket_stream.Receive(external_clients.get_socket(client_id));
  for (int j = 0; j < size; j++)
    for (int i = 0; i < m; i++)
      get_Cp_ref(registers[i] + j) =
          socket_stream.get<typename sint::open_type>();
}

// Receive vector of field element shares over private channel
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_socket_private(int client_id,
    const vector<int>& registers, int size, bool read_macs)
{
  int m = registers.size();
  socket_stream.reset_write_head();
  socket_stream.Receive(external_clients.get_socket(client_id));

  for (int j = 0; j < size; j++)
    for (int i = 0; i < m; i++)
      get_Sp_ref(registers[i] + j).unpack(socket_stream, read_macs);
}


// Read share data from a file starting at file_pos until registers filled.
// file_pos_register is written with new file position (-1 is eof).
// Tolerent to no file if no shares yet persisted.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_shares_from_file(int start_file_posn, int end_file_pos_register, const vector<int>& data_registers) {
  if (not sint::real_shares(P))
    return;

  string filename;
  filename = "Persistence/Transactions-P" + to_string(P.my_num()) + ".data";

  unsigned int size = data_registers.size();

  vector< sint > outbuf(size);

  int end_file_posn = start_file_posn;

  try {
    binary_file_io.read_from_file(filename, outbuf, start_file_posn, end_file_posn);

    for (unsigned int i = 0; i < size; i++)
    {
      get_Sp_ref(data_registers[i]) = outbuf[i];
    }

    write_Ci(end_file_pos_register, (long)end_file_posn);    
  }
  catch (file_missing& e) {
    cerr << "Got file missing error, will return -2. " << e.what() << endl;
    write_Ci(end_file_pos_register, (long)-2);
  }
}

// Append share data in data_registers to end of file. Expects Persistence directory to exist.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::write_shares_to_file(long start_pos,
    const vector<int>& data_registers)
{
  if (not sint::real_shares(P))
    return;

  string filename = binary_file_io.filename(P.my_num());

  unsigned int size = data_registers.size();

  vector< sint > inpbuf (size);

  for (unsigned int i = 0; i < size; i++)
  {
    inpbuf[i] = get_Sp_ref(data_registers[i]);
  }

  binary_file_io.write_to_file(filename, inpbuf, start_pos);
}

template <class T>
void SubProcessor<T>::POpen(const Instruction& inst)
{
  if (inst.get_n())
    check();
  auto& reg = inst.get_start();
  int size = inst.get_size();
  assert(reg.size() % 2 == 0);
  int sz=reg.size() / 2;
  MC.init_open(P, sz * size);
  for (auto it = reg.begin() + 1; it < reg.end(); it += 2)
    for (int i = 0; i < size; i++)
      MC.prepare_open(S[*it + i]);
  MC.exchange(P);
  for (auto it = reg.begin(); it < reg.end(); it += 2)
    for (int i = 0; i < size; i++)
      C[*it + i] = MC.finalize_open();

  if (Proc != 0)
    {
      Proc->sent += sz * size;
      Proc->rounds++;
    }
}

template<class T>
void SubProcessor<T>::muls(const vector<int>& reg, int size)
{
    assert(reg.size() % 3 == 0);
    int n = reg.size() / 3;

    SubProcessor<T>& proc = *this;
    protocol.init_mul();
    for (int i = 0; i < n; i++)
        for (int j = 0; j < size; j++)
        {
            auto& x = proc.S[reg[3 * i + 1] + j];
            auto& y = proc.S[reg[3 * i + 2] + j];
            protocol.prepare_mul(x, y);
        }
    protocol.exchange();
    for (int i = 0; i < n; i++)
        for (int j = 0; j < size; j++)
        {
            proc.S[reg[3 * i] + j] = protocol.finalize_mul();
        }

    protocol.counter += n * size;
}

template<class T>
void SubProcessor<T>::mulrs(const vector<int>& reg)
{
    assert(reg.size() % 4 == 0);
    int n = reg.size() / 4;

    SubProcessor<T>& proc = *this;
    protocol.init_mul();
    for (int i = 0; i < n; i++)
        for (int j = 0; j < reg[4 * i]; j++)
        {
            auto& x = proc.S[reg[4 * i + 2] + j];
            auto& y = proc.S[reg[4 * i + 3]];
            protocol.prepare_mul(x, y);
        }
    protocol.exchange();
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < reg[4 * i]; j++)
        {
            proc.S[reg[4 * i + 1] + j] = protocol.finalize_mul();
        }
        protocol.counter += reg[4 * i];
    }
}

template<class T>
void SubProcessor<T>::dotprods(const vector<int>& reg, int size)
{
    protocol.init_dotprod();
    for (int i = 0; i < size; i++)
    {
        auto it = reg.begin();
        while (it != reg.end())
        {
            auto next = it + *it;
            it += 2;
            while (it != next)
            {
                protocol.prepare_dotprod(S[*it + i], S[*(it + 1) + i]);
                it += 2;
            }
            protocol.next_dotprod();
        }
    }
    protocol.exchange();
    for (int i = 0; i < size; i++)
    {
        auto it = reg.begin();
        while (it != reg.end())
        {
            auto next = it + *it;
            it++;
            S[*it + i] = protocol.finalize_dotprod((next - it) / 2);
            it = next;
        }
    }
}

template<class T>
void SubProcessor<T>::matmuls(const vector<T>& source,
        const Instruction& instruction, size_t a, size_t b)
{
    auto& dim = instruction.get_start();
    auto A = source.begin() + a;
    auto B = source.begin() + b;
    auto C = S.begin() + (instruction.get_r(0));
    assert(A + dim[0] * dim[1] <= source.end());
    assert(B + dim[1] * dim[2] <= source.end());
    assert(C + dim[0] * dim[2] <= S.end());

    protocol.init_dotprod();
    for (int i = 0; i < dim[0]; i++)
        for (int j = 0; j < dim[2]; j++)
        {
            for (int k = 0; k < dim[1]; k++)
                protocol.prepare_dotprod(*(A + i * dim[1] + k),
                        *(B + k * dim[2] + j));
            protocol.next_dotprod();
        }
    protocol.exchange();
    for (int i = 0; i < dim[0]; i++)
        for (int j = 0; j < dim[2]; j++)
            *(C + i * dim[2] + j) = protocol.finalize_dotprod(dim[1]);
}

template<class T>
void SubProcessor<T>::matmulsm(const CheckVector<T>& source,
        const Instruction& instruction, size_t a, size_t b)
{
    auto& dim = instruction.get_start();
    auto C = S.begin() + (instruction.get_r(0));
    assert(C + dim[0] * dim[2] <= S.end());
    assert(Proc);

    protocol.init_dotprod();
    for (int i = 0; i < dim[0]; i++)
    {
        auto ii = Proc->get_Ci().at(dim[3] + i);
        for (int j = 0; j < dim[2]; j++)
        {
            auto jj = Proc->get_Ci().at(dim[6] + j);
            for (int k = 0; k < dim[1]; k++)
            {
                auto kk = Proc->get_Ci().at(dim[4] + k);
                auto ll = Proc->get_Ci().at(dim[5] + k);
                protocol.prepare_dotprod(source.at(a + ii * dim[7] + kk),
                        source.at(b + ll * dim[8] + jj));
            }
            protocol.next_dotprod();
        }
    }
    protocol.exchange();
    for (int i = 0; i < dim[0]; i++)
        for (int j = 0; j < dim[2]; j++)
            *(C + i * dim[2] + j) = protocol.finalize_dotprod(dim[1]);
}

template<class T>
void SubProcessor<T>::conv2ds(const Instruction& instruction)
{
    protocol.init_dotprod();
    auto& args = instruction.get_start();
    int output_h = args[0], output_w = args[1];
    int inputs_h = args[2], inputs_w = args[3];
    int weights_h = args[4], weights_w = args[5];
    int stride_h = args[6], stride_w = args[7];
    int n_channels_in = args[8];
    int padding_h = args[9];
    int padding_w = args[10];
    int batch_size = args[11];
    size_t r0 = instruction.get_r(0);
    size_t r1 = instruction.get_r(1);
    int r2 = instruction.get_r(2);
    int lengths[batch_size][output_h][output_w];
    memset(lengths, 0, sizeof(lengths));
    int filter_stride_h = 1;
    int filter_stride_w = 1;
    if (stride_h < 0)
    {
        filter_stride_h = -stride_h;
        stride_h = 1;
    }
    if (stride_w < 0)
    {
        filter_stride_w = -stride_w;
        stride_w = 1;
    }

    for (int i_batch = 0; i_batch < batch_size; i_batch ++)
    {
        size_t base = r1 + i_batch * inputs_w * inputs_h * n_channels_in;
        assert(base + inputs_w * inputs_h * n_channels_in <= S.size());
        T* input_base = &S[base];
        for (int out_y = 0; out_y < output_h; out_y++)
            for (int out_x = 0; out_x < output_w; out_x++)
            {
                int in_x_origin = (out_x * stride_w) - padding_w;
                int in_y_origin = (out_y * stride_h) - padding_h;

                for (int filter_y = 0; filter_y < weights_h; filter_y++)
                {
                    int in_y = in_y_origin + filter_y * filter_stride_h;
                    if ((0 <= in_y) and (in_y < inputs_h))
                        for (int filter_x = 0; filter_x < weights_w; filter_x++)
                        {
                            int in_x = in_x_origin + filter_x * filter_stride_w;
                            if ((0 <= in_x) and (in_x < inputs_w))
                            {
                                T* pixel_base = &input_base[(in_y * inputs_w
                                        + in_x) * n_channels_in];
                                T* weight_base = &S[r2
                                        + (filter_y * weights_w + filter_x)
                                                * n_channels_in];
                                for (int in_c = 0; in_c < n_channels_in; in_c++)
                                    protocol.prepare_dotprod(pixel_base[in_c],
                                            weight_base[in_c]);
                                lengths[i_batch][out_y][out_x] += n_channels_in;
                            }
                        }
                }

                protocol.next_dotprod();
            }
    }

    protocol.exchange();

    for (int i_batch = 0; i_batch < batch_size; i_batch ++)
    {
        size_t base = r0 + i_batch * output_h * output_w;
        assert(base + output_h * output_w <= S.size());
        T* output_base = &S[base];
        for (int out_y = 0; out_y < output_h; out_y++)
            for (int out_x = 0; out_x < output_w; out_x++)
            {
                output_base[out_y * output_w + out_x] =
                        protocol.finalize_dotprod(
                                lengths[i_batch][out_y][out_x]);
            }
    }
}

template<class T>
void SubProcessor<T>::secure_shuffle(const Instruction& instruction)
{
    typename T::Protocol::Shuffler(S, instruction.get_size(),
            instruction.get_n(), instruction.get_r(0), instruction.get_r(1),
            *this);
}

template<class T>
size_t SubProcessor<T>::generate_secure_shuffle(const Instruction& instruction)
{
    return shuffler.generate(instruction.get_n());
}

template<class T>
void SubProcessor<T>::apply_shuffle(const Instruction& instruction, int handle)
{
    shuffler.apply(S, instruction.get_size(), instruction.get_start()[2],
            instruction.get_start()[0], instruction.get_start()[1], handle,
            instruction.get_start()[4]);
}

template<class T>
void SubProcessor<T>::delete_shuffle(int handle)
{
    shuffler.del(handle);
}

template<class T>
void SubProcessor<T>::inverse_permutation(const Instruction& instruction) {
    shuffler.inverse_permutation(S, instruction.get_size(), instruction.get_start()[0],
                                 instruction.get_start()[1]);
}

template<class T>
void SubProcessor<T>::input_personal(const vector<int>& args)
{
  input.reset_all(P);
  for (size_t i = 0; i < args.size(); i += 4)
    for (int j = 0; j < args[i]; j++)
      {
        if (args[i + 1] == P.my_num())
          input.add_mine(C[args[i + 3] + j]);
        else
          input.add_other(args[i + 1]);
      }
  input.exchange();
  for (size_t i = 0; i < args.size(); i += 4)
    for (int j = 0; j < args[i]; j++)
      S[args[i + 2] + j] = input.finalize(args[i + 1]);
}

/**
 *
 * @tparam T
 * @param args Args contains four arguments
 *      a[0] = the size of the input (and output) vector
 *      a[1] = the player to which to reveal the output
 *      a[2] = the memory address of the input vector (sint) (i.e. the value to reveal)
 *      a[3] = the memory address of the output vector (cint) (i.e. the register to store the revealed value)
 * // TODO: When would there be multiple sets of arguments? (for ... i < args.size(); i += 4 ... )
 */
template<class T>
void SubProcessor<T>::private_output(const vector<int>& args)
{
  typename T::PrivateOutput output(*this);
  for (size_t i = 0; i < args.size(); i += 4)
    for (int j = 0; j < args[i]; j++)
      {
        int player = args[i + 1];
        output.prepare_sending(S.at(args[i + 3] + j), player);
      }
  output.exchange();
  for (size_t i = 0; i < args.size(); i += 4)
    for (int j = 0; j < args[i]; j++)
      C.at(args[i + 2] + j) = output.finalize(args[i + 1]);
}

template<class T>
void SubProcessor<T>::send_personal(const vector<int>& args)
{
  octetStreams to_send(P), to_receive(P);
  for (size_t i = 0; i < args.size(); i += 5)
    if (args[i + 3] == P.my_num())
        for (int j = 0; j < args[i]; j++)
          C[args[i + 4] + j].pack(to_send[args[i + 1]]);
  P.send_receive_all(to_send, to_receive);
  for (size_t i = 0; i < args.size(); i += 5)
    if (args[i + 1] == P.my_num())
        for (int j = 0; j < args[i]; j++)
          C[args[i + 2] + j].unpack(to_receive[args[i + 3]]);
}

template<class sint, class sgf2n>
typename sint::clear Processor<sint, sgf2n>::get_inverse2(unsigned m)
{
  for (unsigned i = inverses2m.size(); i <= m; i++)
    inverses2m.push_back((cint(1) << i).invert());
  return inverses2m[m];
}

template<class sint, class sgf2n>
long Processor<sint, sgf2n>::sync_Ci(size_t i) const
{
  return sync(read_Ci(i));
}

template<class sint, class sgf2n>
long Processor<sint, sgf2n>::sync(long x) const
{
  if (not sint::symmetric)
    {
      // send number to dealer
      if (P.my_num() == 0)
        P.send_long(P.num_players() - 1, x);
      if (not sint::real_shares(P))
        return P.receive_long(0);
    }

  return x;
}

#endif
