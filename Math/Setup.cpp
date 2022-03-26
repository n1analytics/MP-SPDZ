
#include "Math/Setup.h"
#include "Math/gfp.h"
#include "Math/gf2n.h"

#include "Tools/mkpath.h"

#include <fstream>

#include "Math/gfp.hpp"

/*
 * Just setup the primes, doesn't need NTL.
 * Sets idx and m to be used by SHE setup if necessary
 */
bigint SPDZ_Data_Setup_Primes(int lgp)
{
  int idx, m;
  bigint p;
  SPDZ_Data_Setup_Primes(p, lgp, idx, m);
  return p;
}

void SPDZ_Data_Setup_Primes(bigint& p,int lgp,int& idx,int& m)
{
#ifdef VERBOSE
  cerr << "Setting up parameters" << endl;
#endif

  switch (lgp)
    { case -1:
        m=16;
        idx=1;    // Any old figures will do, but need to be for lgp at last
        lgp=32;   // Switch to bigger prime to get parameters
        break;
      case 32:
        m=8192;
        idx=0;
        break;
      case 64:
        m=16384;
        idx=1;
        break;
      case 128:
        m=32768; 
        idx=2;
        break;
      case 256: 
        m=32768;
        idx=3;
        break;
      case 512:
        m=65536;
        idx=4;
        break;
      default:
        m=1;
        idx=0;
#ifdef VERBOSE
        cerr << "no precomputed parameters, trying anyway" << endl;
#endif
        break;
    }
#ifdef VERBOSE
  cerr << "m = " << m << endl;
#endif

  // spdz lgp=128,  m = 32768
  generate_prime(p, lgp, m);
}

bigint generate_prime(int lgp, int m)
{
  bigint p;
  generate_prime(p, lgp, m);
  return p;
}

void generate_prime(bigint& p, int lgp, int m)
{
  if (OnlineOptions::singleton.prime > 0)
    {
      p = OnlineOptions::singleton.prime;
      if (!probPrime(p))
        {
          cerr << p << " is not a prime" << endl;
          exit(1);
        }
      else if (m != 1 and p % m != 1)
        {
          cerr << p
              << " is not compatible with our encryption scheme, must be 1 modulo "
              << m << endl;
          exit(1);
        }
      else
          return;
    }

  bigint u;
  int ex;
  ex = lgp - numBits(m);
  if (ex < 0)
    throw runtime_error(to_string(lgp) + "-bit primes too small "
            "for our parameters");
  u = 1;
  u = (u << ex) * m;
  p = u + 1;
  while (!probPrime(p) || numBits(p) < lgp)
    {
      u = u + m;
      p = u + 1;
    }

#ifdef VERBOSE
  cerr << "\t p = " << p << "  u = " << u << "  :   ";
  cerr << lgp << " <= " << numBits(p) << endl;
#endif
}


void write_online_setup(string dirname, const bigint& p)
{
  if (p == 0)
    throw runtime_error("prime cannot be 0");

  stringstream ss;
  ss << dirname;
  cerr << "Writing to file in " << ss.str() << endl;
  // create preprocessing dir. if necessary
  if (mkdir_p(ss.str().c_str()) == -1)
  {
    cerr << "mkdir_p(" << ss.str() << ") failed\n";
    throw file_error(ss.str());
  }

  // Output the data
  ss << "/Params-Data";
  ofstream outf;
  outf.open(ss.str().c_str());
  outf << p << endl;
  if (!outf.good())
    throw file_error("cannot write to " + ss.str());
}

void check_setup(string dir, bigint pr)
{
  bigint p;
  string filename = dir + "Params-Data";
  ifstream(filename) >> p;
  if (p == 0)
    throw runtime_error("no modulus in " + filename);
  if (p != pr)
    throw runtime_error("wrong modulus in " + filename);
}

string get_prep_sub_dir(const string& prep_dir, int nparties, int log2mod,
        const string& type_short)
{
  string res = prep_dir + "/" + to_string(nparties) + "-" + type_short;
  if (log2mod > 1)
    res += "-" + to_string(log2mod);
  res += "/";
  if (mkdir_p(res.c_str()) < 0)
    throw file_error(res);
  return res;
}
