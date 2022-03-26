#ifndef _EncCommit
#define _EncCommit

// TODO : WHAT???????????
/* Runs the EncCommit protocol
 *  - Input is the condition
 *  - Output mess for this Player
 *    Ciphertext vector for the other players
 */

#include "Networking/Player.h"
#include "FHE/Ciphertext.h"
#include "FHE/Plaintext.h"
#include "Tools/MemoryUsage.h"

class MachineBase;

template<class T,class FD,class S>
class EncCommitBase
{
public:
  size_t volatile_memory;

  const MachineBase* machine;

  EncCommitBase(const MachineBase* machine = 0) : volatile_memory(0), machine(machine) {}
  virtual ~EncCommitBase() {}
  virtual condition get_condition() { return Full; }
  virtual void next(Plaintext<T,FD,S>& mess, Ciphertext& c)
  {
    (void)mess;
    (void)c;
    throw runtime_error("don't use EncCommitBase");
  }

  virtual size_t report_size(ReportType type) { (void)type; return 0; }
  virtual void report_size(ReportType type, MemoryUsage& res)
  { (void)type; (void)res; }
  virtual size_t get_volatile() { return volatile_memory; }
};

template <class FD>
using EncCommitBase_ = EncCommitBase<typename FD::T, FD, typename FD::S>;

class MachineBase;

template<class T,class FD,class S>
class EncCommit : public EncCommitBase<T,FD,S>
{
  const Player *P; 
  const FHE_PK *pk;
  bool covert;
  condition cond;

  // Covert data
  int num_runs;

  // Pick t=12 since this is the twice number of EncCommits needed in a run
  // of the multiplication triples production. It also provides a nice
  // balance between time and memory
  static const int DEFAULT_T = 12;
  static const int DEFAULT_B = 16;
  static const int DEFAULT_DELTA = 256;

  // Active data
  //   - Need to know the thread as we save lots of data to disk
  //     and we need to distinguish one threads data from anothers
  int TT,b,t,thread,M;

  mutable int cnt;
  mutable vector< vector<Ciphertext> > c;
  mutable vector< Plaintext<T,FD,S> > m;

  mutable map<string, Timer> timers;

  // Covert routines
  void next_covert(Plaintext<T,FD,S>& mess,vector<Ciphertext>& C) const;

  // Active routines
  void Create_More() const;
  void next_active(Plaintext<T,FD,S>& mess,vector<Ciphertext>& C) const;

  public:

  static bigint active_mask(int phim, int TT = DEFAULT_T * DEFAULT_B,
      int delta = DEFAULT_DELTA)
  { return 4 * delta * TT * phim; }
  static bigint active_slack(int phim) { return 2 * active_mask(phim); }

  EncCommit() : cnt(-1) { ; }
  ~EncCommit();

  // for compatibility
  EncCommit(const PlayerBase& P, const FHE_PK& pk, const FD& FTD,
            map<string, Timer>& timers, const MachineBase& machine,
            int thread_num) :
            EncCommit()
  { (void)P; (void)pk; (void)FTD; (void)timers; (void)machine; (void)thread_num; }

  // Set up for covert
  void init(const Player& PP,const FHE_PK& fhepk,int params,condition cc);
  EncCommit(const Player& PP,const FHE_PK& fhepk,int params,condition cc)
     { init(PP,fhepk,params,cc); }

  // Set up for active (40 bits security)
  void init(const Player& PP,const FHE_PK& fhepk,condition cc,const FD& PFieldD,int thr);
  EncCommit(const Player& PP,const FHE_PK& fhepk,condition cc,const FD& PFieldD,int thr)
      { init(PP,fhepk,cc,PFieldD,thr); }

  condition get_condition() { return cond; }

  bool has_left() const { return cnt >= 0; }

  void next(Plaintext<T,FD,S>& mess, vector<Ciphertext>& C)
    {
      if (C.size() != 0 and pk->get_params() != C[0].get_params())
        C.clear();
      if (covert)
	{ next_covert(mess,C); }
      else
        { next_active(mess,C); }
    }

  void next(Plaintext<T,FD,S>& mess, Ciphertext& c)
    {
      vector<Ciphertext> C;
      next(mess, C);

      c = C[0];
      for (int i = 1; i < P->num_players(); i++)
        add(c, c, C[i]);
    }
};

template <class FD>
using EncCommit_ = EncCommit<typename FD::T, FD, typename FD::S>;

template <class FD>
void covert_generation(Plaintext_<FD>& mess, vector<Ciphertext>& C,
    const vector<const FHE_PK*>& pks, const Player* P, int num_runs, condition cond);

template <class FD>
void generate_mac_key(typename FD::T& key_share, Ciphertext& key, const FD& FieldD,
    FHE_PK& pk, Player& P, int covert)
{
  EncCommit_<FD> ec(P, pk, covert, Diagonal);
  Plaintext_<FD> mess(FieldD);
  ec.next(mess, key);

  /* Check all OK */
  P.Check_Broadcast();
  key_share = mess.element(0);
}

#endif

