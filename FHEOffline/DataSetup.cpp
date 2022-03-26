/*
 * DataSetup.cpp
 *
 */

#include <FHEOffline/DataSetup.h>
#include "FHEOffline/DistKeyGen.h"
#include "Protocols/fake-stuff.h"
#include "FHE/NTL-Subs.h"
#include "Tools/benchmarking.h"
#include "Tools/Bundle.h"
#include "PairwiseSetup.h"
#include "Proof.h"
#include "SimpleMachine.h"
#include "PairwiseMachine.h"

#include <iostream>
using namespace std;

#include "Protocols/fake-stuff.hpp"

template <class FD>
// 初始化，没做什么东西
PartSetup<FD>::PartSetup() :
    pk(params, 0), sk(params, 0), calpha(params)
{
}

DataSetup::DataSetup()
{
}

DataSetup& DataSetup::operator=(const DataSetup& other)
{
  setup_p = other.setup_p;
  setup_2 = other.setup_2;
  return *this;
}

// 创建公私钥，mac认证密钥的加密
template <class FD>
void PartSetup<FD>::generate_setup(int n_parties, int plaintext_length, int sec,
    int slack, bool round_up)
{
    // 安全参数 最低40
  sec = max(sec, 40);

  Parameters(n_parties, plaintext_length, sec, slack, round_up).generate_setup(params, FieldD);
  params.set_sec(sec);
  // 公私钥生成，认证密钥的加密
  pk = FHE_PK(params, FieldD.get_prime());
  sk = FHE_SK(params, FieldD.get_prime());
  calpha = Ciphertext(params);
}

template <class FD>
void PartSetup<FD>::fake(vector<FHE_SK>& sks, vector<T>& alphais,
        int nplayers, bool distributed, bool check_security)
{
  if (check_security)
    insecure("global key generation");
  if (distributed)
      cout << "Faking distributed key generation" << endl;
  else
      cout << "Faking key generation with extra noise" << endl;
  PRNG G;
  G.ReSeed();
  pk = FHE_PK(params, FieldD.get_prime());
  FHE_SK sk(params, FieldD.get_prime());
  calpha = Ciphertext(params);
  sks.resize(nplayers, pk);
  alphais.resize(nplayers);

  if (distributed)
      DistKeyGen::fake(pk, sks, FieldD.get_prime(), nplayers);
  else
  {
      Rq_Element sk = FHE_SK(pk).s();
      for (int i = 0; i < nplayers; i++)
      {
          Rq_Element ski = pk.sample_secret_key(G);
          sks[i].assign(ski);
          sk += ski;
      }
      pk.KeyGen(sk, G, nplayers);
  }

  for (int i = 0; i < nplayers; i++)
    {
      Plaintext_<FD> m(FieldD);
      m.randomize(G,Diagonal);
      Ciphertext calphai = pk.encrypt(m);
      calpha += calphai;
      alphais[i] = m.element(0);
    }
}

template <class FD>
void PartSetup<FD>::fake(vector<PartSetup<FD> >& setups, int nplayers,
        bool distributed, bool check_security)
{
    vector<FHE_SK> sks;
    vector<T> alphais;
    fake(sks, alphais, nplayers, distributed, check_security);
    setups.clear();
    setups.resize(nplayers, *this);
    for (int i = 0; i < nplayers; i++)
    {
        setups[i].sk = sks[i];
        setups[i].alphai = alphais[i];
    }
}

template <class FD>
void PartSetup<FD>::insecure_debug_keys(vector<PartSetup<FD> >& setups, int nplayers, bool simple_pk)
{
    cout << "generating INSECURE keys for debugging" << endl;
    setups.clear();
    Rq_Element zero(params, evaluation, evaluation),
            one(params, evaluation, evaluation);
    zero.assign_zero();
    one.assign_one();
    PRNG G;
    G.ReSeed();
    if (simple_pk)
        pk.assign(zero, zero, zero, zero - one);
    else
        pk.KeyGen(one, G, nplayers);
    setups.resize(nplayers, *this);
    setups[0].sk.assign(one);
    for (int i = 1; i < nplayers; i++)
        setups[i].sk.assign(zero);
}

template<class FD>
void PartSetup<FD>::output(Names& N)
{
    // Write outputs to file
    string dir = get_prep_sub_dir<Share<typename FD::T>>(N.num_players());
    write_online_setup(dir, FieldD.get_prime());
    write_mac_key(dir, N.my_num(), N.num_players(), alphai);
}

template <class FD>
void PartSetup<FD>::pack(octetStream& os)
{
    os.append((octet*)"PARTSETU", 8);
    params.pack(os);
    FieldD.pack(os);
    pk.pack(os);
    sk.pack(os);
    calpha.pack(os);
    alphai.pack(os);
}

template <class FD>
void PartSetup<FD>::unpack(octetStream& os)
{
    char tag[8];
    os.consume((octet*) tag, 8);
    if (memcmp(tag, "PARTSETU", 8))
      throw runtime_error("invalid serialization of setup");
    params.unpack(os);
    FieldD.unpack(os);
    pk = {params, FieldD};
    sk = pk;
    calpha = params;
    pk.unpack(os);
    sk.unpack(os);
    calpha.unpack(os);
    init_field();
    alphai.unpack(os);
}

template <>
void PartSetup<FFT_Data>::init_field()
{
    gfp::init_field(FieldD.get_prime());
}

template <>
void PartSetup<P2Data>::init_field()
{
}

template <class FD>
void PartSetup<FD>::check(int sec) const
{
    sec = max(sec, 40);
    if (abs(sec - params.secp()) > 2)
        throw runtime_error("security parameters vary too much between protocol and distributed decryption");
    sk.check(params, pk, FieldD.get_prime());
}

template <class FD>
bool PartSetup<FD>::operator!=(const PartSetup<FD>& other)
{
    if (params != other.params or FieldD != other.FieldD or pk != other.pk
            or sk != other.sk or calpha != other.calpha
            or alphai != other.alphai)
        return true;
    else
        return false;
}

template<class FD>
void PartSetup<FD>::secure_init(Player& P, MachineBase& machine,
    int plaintext_length, int sec)
{
    ::secure_init(*this, P, machine, plaintext_length, sec, params);
}

template<class FD>
void PartSetup<FD>::generate(Player& P, MachineBase&, int plaintext_length,
        int sec)
{
    generate_setup(P.num_players(), plaintext_length, sec,
            INTERACTIVE_SPDZ1_SLACK, false);
}

template<class FD>
void PartSetup<FD>::check(Player& P, MachineBase& machine)
{
    Bundle<octetStream> bundle(P);
    bundle.mine.store(machine.extra_slack);
    auto& os = bundle.mine;
    params.pack(os);
    FieldD.hash(os);
    pk.pack(os);
    calpha.pack(os);
    bundle.compare(P);
}

template<class FD>
void PartSetup<FD>::covert_key_generation(Player& P, MachineBase&, int num_runs)
{
    Run_Gen_Protocol(pk, sk, P, num_runs, false);
}

template<class FD>
void PartSetup<FD>::covert_mac_generation(Player& P, MachineBase&, int num_runs)
{
    generate_mac_key(alphai, calpha, FieldD, pk, P,
            num_runs);
}

template<class FD>
void PartSetup<FD>::key_and_mac_generation(Player& P,
        MachineBase& machine, int num_runs, true_type)
{
    covert_key_generation(P, machine, num_runs);
    covert_mac_generation(P, machine, num_runs);
}

template class PartSetup<FFT_Data>;
template class PartSetup<P2Data>;
