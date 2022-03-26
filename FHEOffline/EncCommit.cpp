
#include "Tools/Subroutines.h"
#include "Tools/Exceptions.h"
#include "Tools/random.h"

#include "EncCommit.h"
#include "FHE/P2Data.h"

#include <math.h>

#include <fstream>
using namespace std;

#include "Math/modp.hpp"

// XXXX File_prefix is only used for active code
#ifndef file_prefix
#define file_prefix "/tmp/"
#endif


template<class T, class FD, class S>
EncCommit<T, FD, S>::~EncCommit()
{
  for (auto& x : timers)
    cout << x.first << ": " << x.second.elapsed() << endl;
}


// Set up for covert
template<class T,class FD,class S>
void EncCommit<T,FD,S>::init(const Player& PP,const FHE_PK& fhepk,int params,condition cc)
{ 
  P=&PP; pk=&fhepk; 
  num_runs=params; cond=cc;
  covert=true;
}


// Set up for active
template<class T,class FD,class S>
void EncCommit<T,FD,S>::init(const Player& PP,const FHE_PK& fhepk,condition cc,const FD& FieldD,int thr)
{ 
  covert=false;
  thread=thr;

  const FHE_Params& params=fhepk.get_params();

  t = DEFAULT_T;
  b = DEFAULT_B;
  TT = b * t;
  // Probability of cheating in one run is...
  //    ee:=(1-b)*log(t)/log(2.0)-b*log(1+log(log(2)))/log(2.0)+log(M)/log(2);
  // Valid prover could fail, so we allow M failures giving
  // prob of valid prover failing of  (1/delta)^M
  M=5;

  P=&PP; pk=&fhepk; cond=cc;     
 

  // Currently cannot deal with cond!=Full for active adversaries
  if (cond!=Full)
    { throw not_implemented(); }

  c.resize(t);
  m.resize(t, Plaintext<T,FD,S>(FieldD));
  for (int i=0; i<t; i++)
    { c[i].resize((*P).num_players(),Ciphertext(params)); 
      m[i].assign_zero(Polynomial);
    }
  cnt=-1;
}



template<class T,class FD,class S>
void EncCommit<T,FD,S>::next_covert(Plaintext<T,FD,S>& mess, vector<Ciphertext>& C) const
{
  covert_generation(mess, C, {size_t(P->num_players()), pk}, P, num_runs, cond);
}

template <class FD>
void covert_generation(Plaintext_<FD>& mess, vector<Ciphertext>& C,
    const vector<const FHE_PK*>& pks, const Player* P, int num_runs, condition cond)
{
  const FHE_Params& params=(*pks[0]).get_params();

  /* Commit to the seeds */
  vector< vector<octetStream> > seeds(num_runs, vector<octetStream>((*P).num_players()));
  vector< vector<octetStream> > Comm_seeds(num_runs,  vector<octetStream>((*P).num_players()));
  vector<octetStream> Open_seeds(num_runs);

  vector<PRNG> G(num_runs);
  Commit_To_Seeds(G,seeds,Comm_seeds,Open_seeds,*P,num_runs);

  // Generate the messages and ciphertexts
  vector< Plaintext_<FD> > m(num_runs,mess.get_field());
  vector<Ciphertext>   c(num_runs,params);
  Random_Coins rc(params);
  for (int i=0; i<num_runs; i++)
    { m[i].randomize(G[i],cond);
      rc.generate(G[i]);
      (*pks[P->my_num()]).encrypt(c[i],m[i],rc);
      //cout << "xxxxxxxxxxxxxxxxxxxxx" << endl;
      //cout << i << "\t" << (*P).socket(P.my_num()) << endl;
      //cout << i << "\t" << m[i] << endl;
      //cout << i << "\t" << rc << endl;
      //cout << i << "\t" << c[i] << endl;
    }

  // Broadcast and receive ciphertexts
  vector< vector<octetStream> > ctx(num_runs, vector<octetStream> ((*P).num_players()));
  for (int i=0; i<num_runs; i++)
    { c[i].pack(ctx[i][(*P).my_num()]);
      (*P).Broadcast_Receive(ctx[i]);
    }

  // Compute and commit to the challenge value
  vector<unsigned int> e((*P).num_players());
  vector<octetStream> Comm_e((*P).num_players());
  vector<octetStream> Open_e((*P).num_players());
  Commit_To_Challenge(e,Comm_e,Open_e,*P,num_runs);
  
  // Open challenge
  int challenge=Open_Challenge(e,Open_e,Comm_e,*P,num_runs);

  // Open seeds, but not the challenge run
  Open(seeds,Comm_seeds,Open_seeds,*P,num_runs,challenge);

  // Now check all the prior executions
  Plaintext_<FD> mm(mess.get_field());
  Ciphertext cc(params);
  octetStream occ;
  for (int i=0; i<num_runs; i++)
    { if (i!=challenge)
        { for (int j=0; j<(*P).num_players(); j++)
            { if (j!=(*P).my_num())
	        { PRNG G;
                  G.SetSeed(seeds[i][j].get_data());
	          //cout << "GOT SEED : " << i << " " << j << " " << P.socket(j) << seeds[i][j] << endl;
                  mm.randomize(G,cond);
                  rc.generate(G);
                  (*pks[j]).encrypt(cc,mm,rc);
                  occ.reset_write_head();
                  cc.pack(occ);
                  if (!occ.equals(ctx[i][j]))
                    { //cout << "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz" << endl;
                      //cout << i << " " << j << " " << P.socket(j) << " " << mm << endl;
                      //cout << i << " " << j << " " << P.socket(j) << " " << rc << endl;
                      //cout << i << " " << j << " " << P.socket(j) << " " << cc << endl;
                      throw bad_enccommit(); 
                    }
		}
	    }
        }
    }

  // Set output
  mess=m[challenge];
  C.resize((*P).num_players(),Ciphertext(params));
  for (int j=0; j<(*P).num_players(); j++)
    { if (j!=(*P).my_num())
	{ C[j].unpack(ctx[challenge][j]); }
      else
        { C[j]=c[challenge]; }
    }
}




template<class T,class FD,class S>
void EncCommit<T,FD,S>::next_active(Plaintext<T,FD,S>& mess, vector<Ciphertext>& C) const
{
  const FHE_Params& params=(*pk).get_params();
  cout << "121231231:" << Ciphertext(params).c1().lev << endl;
  if (not has_left()) { Create_More(); }
  mess=m[cnt];
  C.resize((*P).num_players(),Ciphertext(params));
  for (int i=0; i<(*P).num_players(); i++)
    { C[i]=c[cnt][i]; }
  cnt--;
}

bool check_norm(const Rq_Element& value, const bigint& bound)
{
  bool res = (value.infinity_norm() > bound);
  if (res)
    cout << "out of bound by (log) "
        << log2(value.infinity_norm().get_d() / bound.get_d()) << endl;
  return res;
}

template<class T,class FD,class S>
void EncCommit<T,FD,S>::Create_More() const
{
  const FHE_Params& params=(*pk).get_params();

  // Tweak by using p rather than p/2, as we do not center the ciphertexts
  int rhoi=(int) (6*params.get_R());
  bigint comm = active_mask(params.phi_m(), b, t);
  bigint Bound1=m[0].get_field().get_prime()*comm;
  bigint Bound2=comm;
  bigint Bound3=rhoi*comm;
  bigint UBound1=2*(Bound1-m[0].get_field().get_prime());
  bigint UBound2=2*(Bound2-1);
  bigint UBound3=2*(Bound3-rhoi);

  int my_num=(*P).my_num();
  /* First generate the good ciphertexts and messages for myself */
  PRNG G;
  G.ReSeed();
  vector<Random_Coins> rc(t,params);
  for (int i=0; i<t; i++)
    { m[i].randomize(G,cond);
      rc[i].generate(G);
      (*pk).encrypt(c[i][my_num],m[i],rc[i]);
    }

  // Broadcast and receive ciphertexts "c"
  vector< vector<octetStream> > ctx_c(t, vector<octetStream> ((*P).num_players()));
  for (int i=0; i<t; i++)
    { c[i][my_num].pack(ctx_c[i][my_num]);
      (*P).Broadcast_Receive(ctx_c[i]);
      for (int j=0; j<(*P).num_players(); j++)
	{ if (j!=my_num)
	    { c[i][j].unpack(ctx_c[i][j]); }
        }
    }

  vector< vector<octetStream> > seeds(2*TT, vector<octetStream>((*P).num_players()));
  vector< vector<octetStream> > Comm_seeds(2*TT,  vector<octetStream>((*P).num_players()));
  vector< octetStream > ctx_Delta((*P).num_players());
  Ciphertext ctx_C(params);
  vector<octetStream> Open_seeds(2*TT);
  vector<PRNG> Gseed(2*TT);
  vector<int> open(2*TT);
  vector<Rq_Element> z(4,Rq_Element(params.FFTD(),polynomial,polynomial));
  vector<octetStream> Comm_data((*P).num_players());

  char filename[1024];
  bool fail=true;
  int iter=0;
  while (fail && iter<M)
    { fail=false;
      // Now commit to seeds for the Delta's
      cout << "\t Create_More : iteration number = " << iter << endl;
      for (int i=0; i<2*TT; i++)
        { for (int j=0; j<(*P).num_players(); j++)
	    { seeds[i][j].reset_write_head(); 
	      Comm_seeds[i][j].reset_write_head(); 
            } 
	  Open_seeds[i].reset_write_head(); 
        }
      for (int j=0; j<(*P).num_players(); j++)
	{ Comm_data[j].reset_write_head();  }
      
      Commit_To_Seeds(Gseed,seeds,Comm_seeds,Open_seeds,*P,2*TT);

      // Now generate and transmit/receive the Delta's
      Rq_Element m_Delta(Rq_Element(params.FFTD(),polynomial,polynomial));
      Random_Coins rc_Delta(params);
      for (int i=0; i<2*TT; i++)
        { if (cond!=Full) 
             { throw not_implemented(); }
          else          
             { m_Delta.from(UniformGenerator<bigint>(Gseed[i],numBits(Bound1))); }
          rc_Delta.generateUniform(Gseed[i],Bound2,Bound3,Bound3);
          Ciphertext Delta(params);
          (*pk).quasi_encrypt(Delta,m_Delta,rc_Delta);
          for (int j=0; j<(*P).num_players(); j++)
	    { ctx_Delta[j].reset_write_head(); }
          Delta.pack(ctx_Delta[my_num]);
          (*P).Broadcast_Receive(ctx_Delta);
           
          // Output the ctx_Delta to a file
          sprintf(filename,"%sctx_Delta-%d-%d-%d",file_prefix,my_num,i,thread);
          ofstream outf(filename);
          for (int j=0; j<(*P).num_players(); j++)
	    {
              ctx_C.unpack(ctx_Delta[j]);
              timers["ciphertext writing"].start();
              ctx_C.output(outf);
              timers["ciphertext writing"].stop();
            }
          outf.close();
        }

      // Generate a random seed and use it to determine which Delta's to open
      octet seed[SEED_SIZE];
      Create_Random_Seed(seed,*P,SEED_SIZE);
      PRNG RG; RG.SetSeed(seed);
    
      Rq_Element mm(params.FFTD(),polynomial,polynomial);
      Ciphertext cc(params);
      Random_Coins rr(params);
      for (int i=0; i<2*TT; i++) { open[i]=0; }
      for (int i=0; i<TT; i++)
        { int j=RG.get_uint()%(2*TT);
          while (open[j]==1) { j=RG.get_uint()%(2*TT); }
          open[j]=1;
        }
    
      // Now open the seeds we have decided to open
      Open(seeds,Comm_seeds,Open_seeds,open,*P,2*TT);
    
      // Now check all opened Delta's are OK for all players
      octetStream occ,ctx_D;
      for (int i=0; i<2*TT; i++)
        { if (open[i]==1)
           { sprintf(filename,"%sctx_Delta-%d-%d-%d",file_prefix,my_num,i,thread);
             ifstream inpf(filename);
             for (int j=0; j<(*P).num_players(); j++)
               {
                 timers["ciphertext reading"].start();
                 ctx_C.input(inpf);
                 timers["ciphertext reading"].stop();
                 if (j!=my_num)
                   { PRNG G;
                     G.SetSeed(seeds[i][j].get_data());
                     if (cond!=Full) 
                        { throw not_implemented(); }
                     else   
	                { mm.from(UniformGenerator<bigint>(G, numBits(Bound1))); }
                     rr.generateUniform(G,Bound2,Bound3,Bound3);
                     (*pk).quasi_encrypt(cc,mm,rr);
                     occ.reset_write_head();
                     cc.pack(occ);
                     ctx_D.reset_write_head();
                     ctx_C.pack(ctx_D);
                     if (!occ.equals(ctx_D))
                        { cout << "Error opening position " << i << endl; 
                          // Can only get here if Prover is trying to cheat so abort
                          throw bad_enccommit(); 
                        }
                   }
	       }
             inpf.close();
           }
        }

      // Now move all unopened values of ctx_Delta,seeds to the first TT positions
      //   - Done via index
      int count=0;
      vector<int> index(TT);
      for (int i=0; i<2*TT; i++)
        { if (open[i]==0)
	    { index[count]=i;
              count++;
            }
        }

      for (int i=0; i<t && !fail; i++)
        { for (int j=0; j<b && !fail; j++)
	    { // Step a
              PRNG G;
              G.SetSeed(seeds[index[b*i+j]][my_num].get_data());
              if (cond!=Full) 
                { throw not_implemented();
                }
              else   
	        { m_Delta.from(UniformGenerator<bigint>(G, numBits(Bound1))); }
              rc_Delta.generateUniform(G,Bound2,Bound3,Bound3);
              
              auto vm=m[i].get_iterator();
              z[0].from(vm);
              add(z[0],z[0],m_Delta);
              add(rr,rc[i],rc_Delta);
              z[1]=rr.u();
              z[2]=rr.v();
              z[3]=rr.w();
	      // Step b
              if (check_norm(z[0], UBound1)) { cout << "F0:" << i << " " << j <<  endl; fail=true; }
              if (check_norm(z[1], UBound2)) { cout << "F1:" << i << " " << j << endl; fail=true; }
              if (check_norm(z[2], UBound3)) { cout << "F2:" << i << " " << j <<  endl; fail=true; }
              if (check_norm(z[3], UBound3)) { cout << "F3:" << i << " " << j <<  endl; fail=true; }
              // Set my data to transmit
              Comm_data[my_num].reset_write_head(); 
              if (!fail)
                 { z[0].pack(Comm_data[my_num]);
                   z[1].pack(Comm_data[my_num]);
                   z[2].pack(Comm_data[my_num]);
                   z[3].pack(Comm_data[my_num]);
	         }
              (*P).Broadcast_Receive(Comm_data);
	      Ciphertext enc1(params),enc2(params),eDelta(params);
              octetStream oe1,oe2;

              sprintf(filename,"%sctx_Delta-%d-%d-%d",file_prefix,my_num,index[b*i+j],thread);
              ifstream inpf(filename);
              for (int k=0; k<(*P).num_players(); k++)
                {
                  timers["ciphertext reading"].start();
                  ctx_C.input(inpf);
                  timers["ciphertext reading"].stop();
                  if (k!=my_num)
	            { if (Comm_data[k].get_length()==0)
	                { fail=true; }
                      else
	                { z[0].unpack(Comm_data[k]);
	                  z[1].unpack(Comm_data[k]);
	                  z[2].unpack(Comm_data[k]);
	                  z[3].unpack(Comm_data[k]);
	                  // Check size (any wrong means proving cheating)
                          if (z[0].infinity_norm()>UBound1) { cout << "R0" << endl; throw bad_enccommit(); }
                          if (z[1].infinity_norm()>UBound2) { cout << "R1" << endl; throw bad_enccommit(); }
                          if (z[2].infinity_norm()>UBound3) { cout << "R2" << endl; throw bad_enccommit(); }
                          if (z[3].infinity_norm()>UBound3) { cout << "R3" << endl; throw bad_enccommit(); }
	                  // Need to encrypt with z0,..,z1 and check equal to ci+delta_{b*i+j}
	                  rr.assign(z[1],z[2],z[3]);
                          (*pk).quasi_encrypt(enc1,z[0],rr);
                          add(enc2,c[i][k],ctx_C);
	                  enc1.pack(oe1);
	                  enc2.pack(oe2);
                          // If this is not true then we have a cheating prover
	                  if (!oe1.equals(oe2))
	                    { cout << "EFail " << k << endl; throw bad_enccommit(); }
                        }
                    }
                }
              inpf.close();
            }
        }
      if (fail) { iter++; }
    }
  if (fail)
    { throw bad_enccommit(); }

  cnt=t-1;
}


template class EncCommit<gfp,FFT_Data,bigint>;
template class EncCommit<gf2n_short,P2Data,int>;

template class EncCommitBase<gfp,FFT_Data,bigint>;
template class EncCommitBase<gf2n_short,P2Data,int>;

