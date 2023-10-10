#ifndef _Zp_Data
#define _Zp_Data

/* Class to define helper information for a Zp element
 *
 * Basically the data needed for Montgomery operations 
 *
 * Almost all data is public as this is basically a container class
 *
 */

#include "Math/config.h"
#include "Math/bigint.h"
#include "Math/mpn_fixed.h"
#include "Tools/random.h"
#include "Tools/intrinsics.h"
#include "Tools/Lock.h"

#include <iostream>
using namespace std;

#ifndef MAX_MOD_SZ
   #if defined(GFP_MOD_SZ) and GFP_MOD_SZ > 11
     #define MAX_MOD_SZ GFP_MOD_SZ
   #else
     #define MAX_MOD_SZ 11
  #endif
#endif

template<int L> class modp_;

class Zp_Data
{
  bool        montgomery;  // True if we are using Montgomery arithmetic
  mp_limb_t   R[MAX_MOD_SZ],R2[MAX_MOD_SZ],R3[MAX_MOD_SZ],pi;
  // extra limb needed for Montgomery multiplication
  mp_limb_t   prA[MAX_MOD_SZ+1];
  int         t;           // More Montgomery data
  mp_limb_t   overhang;
  Lock        lock;
  mutable bigint shanks_y, shanks_q_half;
  mutable int    shanks_r;

  template <int T>
  void Mont_Mult_(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y) const;
  void Mont_Mult(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y) const;
  void Mont_Mult_switch(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y) const;
  void Mont_Mult(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y, int t) const;
  void Mont_Mult_variable(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y) const
  { Mont_Mult(z, x, y, get_t()); }
  void Mont_Mult_max(mp_limb_t* z, const mp_limb_t* x, const mp_limb_t* y,
      int max_t) const;

  public:

  bigint       pr;
  bigint       pr_half;
  mp_limb_t    mask;
  size_t       pr_byte_length;
  size_t       pr_bit_length;

  void assign(const Zp_Data& Zp);
  void init(const bigint& p,bool mont=true);
  int get_t() const { assert(t > 0); return t; }
  const mp_limb_t* get_prA() const { return prA; }
  bool get_mont() const { return montgomery; }
  mp_limb_t overhang_mask() const;

  void pack(octetStream& o) const;
  void unpack(octetStream& o);

  // This one does nothing, needed so as to make vectors of Zp_Data
  Zp_Data() :
      montgomery(0), pi(0), mask(0), pr_byte_length(0), pr_bit_length(0)
  {
    t = MAX_MOD_SZ;
    overhang = 0;
    shanks_r = 0;
  }

  // The main init funciton
  Zp_Data(const bigint& p,bool mont=true)
    { init(p,mont); }

  template <int T>
  void Add(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const;
  void Add(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const;
  template <int T>
  void Sub(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const;
  void Sub(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const;

  bool operator!=(const Zp_Data& other) const;
  bool operator==(const Zp_Data& other) const;

  void get_shanks_parameters(bigint& y, bigint& q_half, int& r) const;

   template<int L> friend void to_modp(modp_<L>& ans,int x,const Zp_Data& ZpD);
   template<int L> friend void to_modp(modp_<L>& ans,const mpz_class& x,const Zp_Data& ZpD);

   template<int L> friend void Add(modp_<L>& ans,const modp_<L>& x,const modp_<L>& y,const Zp_Data& ZpD);
   template<int L> friend void Sub(modp_<L>& ans,const modp_<L>& x,const modp_<L>& y,const Zp_Data& ZpD);
   template<int L> friend void Mul(modp_<L>& ans,const modp_<L>& x,const modp_<L>& y,const Zp_Data& ZpD);
   template<int L> friend void Sqr(modp_<L>& ans,const modp_<L>& x,const Zp_Data& ZpD);
   template<int L> friend void Negate(modp_<L>& ans,const modp_<L>& x,const Zp_Data& ZpD);
   template<int L> friend void Inv(modp_<L>& ans,const modp_<L>& x,const Zp_Data& ZpD);

   template<int L> friend void Power(modp_<L>& ans,const modp_<L>& x,int exp,const Zp_Data& ZpD);
   template<int L> friend void Power(modp_<L>& ans,const modp_<L>& x,const bigint& exp,const Zp_Data& ZpD);

   template<int L> friend void assignOne(modp_<L>& x,const Zp_Data& ZpD);
   template<int L> friend void assignZero(modp_<L>& x,const Zp_Data& ZpD);
   template<int L> friend bool isZero(const modp_<L>& x,const Zp_Data& ZpD);
   template<int L> friend bool isOne(const modp_<L>& x,const Zp_Data& ZpD);
   template<int L> friend bool areEqual(const modp_<L>& x,const modp_<L>& y,const Zp_Data& ZpD);

   template<int L> friend class modp_;

   friend ostream& operator<<(ostream& s,const Zp_Data& ZpD);
   friend istream& operator>>(istream& s,Zp_Data& ZpD);
};


inline mp_limb_t Zp_Data::overhang_mask() const
{
  return overhang;
}

template<>
inline void Zp_Data::Add<0>(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
  mp_limb_t carry = mpn_add_n_with_carry(ans,x,y,t);
  if (carry!=0 || mpn_cmp(ans,prA,t)>=0)
    { mpn_sub_n_borrow(ans,ans,prA,t); }
}

template<>
inline void Zp_Data::Add<1>(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
#if defined(__clang__) || !defined(__x86_64__)
  Add<0>(ans, x, y);
#else
  *ans = *x + *y;
  asm goto ("jc %l[sub]" :::: sub);
  if (mpn_cmp(ans, prA, 1) >= 0)
 sub:
      *ans -= *prA;
#endif
}

template<>
inline void Zp_Data::Add<2>(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
#if defined(__clang__) || !defined(__x86_64__)
  Add<0>(ans, x, y);
#else
  __uint128_t a, b, p;
  memcpy(&a, x, sizeof(__uint128_t));
  memcpy(&b, y, sizeof(__uint128_t));
  memcpy(&p, prA, sizeof(__uint128_t));
  __uint128_t c = a + b;
  asm goto ("jc %l[sub]" :::: sub);
  if (c >= p)
 sub:
      c -= p;
  memcpy(ans, &c, sizeof(__uint128_t));
#endif
}

template<int T>
inline void Zp_Data::Add(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
  mp_limb_t carry = mpn_add_fixed_n_with_carry<T>(ans,x,y);
  if (carry!=0 || mpn_cmp(ans,prA,T)>=0)
    { mpn_sub_n_borrow(ans,ans,prA,T); }
}

inline void Zp_Data::Add(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
  switch (t)
  {
#define X(L) case L: Add<L>(ans, x, y); break;
  X(1) X(2) X(3) X(4) X(5)
#undef X
  default:
    return Add<0>(ans, x, y);
  }
}

template <int T>
inline void Zp_Data::Sub(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
  mp_limb_t tmp[T];
  mp_limb_t borrow = mpn_sub_fixed_n_borrow<T>(tmp, x, y);
  if (borrow != 0)
    mpn_add_fixed_n<T>(ans, tmp, prA);
  else
    inline_mpn_copyi(ans, tmp, T);
}

template <>
inline void Zp_Data::Sub<0>(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
  mp_limb_t borrow = mpn_sub_n_borrow(ans,x,y,t);
  if (borrow!=0)
    mpn_add_n_with_carry(ans,ans,prA,t);
}

inline void Zp_Data::Sub(mp_limb_t* ans,const mp_limb_t* x,const mp_limb_t* y) const
{
  switch (t)
  {
#define X(L) case L: Sub<L>(ans, x, y); break;
  X(1) X(2) X(3) X(4) X(5)
#undef X
  default:
    Sub<0>(ans, x, y);
    break;
  }
}

template <int T>
inline void Zp_Data::Mont_Mult_(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y) const
{
#ifdef __BMI2__
  mp_limb_t ans[2*MAX_MOD_SZ+1],u;
  inline_mpn_zero(ans + T + 1, T);
  // First loop
  u=x[0]*y[0]*pi;
  mpn_mul_1_fixed<T + 1, T>(ans,y,x[0]);
  mpn_addmul_1_fixed_<T + 2, T + 1>(ans,prA,u);
  for (int i=1; i<T; i++)
    { // u=(ans0+xi*y0)*pd
      u=(ans[i]+x[i]*y[0])*pi;
      // ans=ans+xi*y+u*pr
      mpn_addmul_1_fixed_<T + 2, T>(ans+i,y,x[i]);
      mpn_addmul_1_fixed_<T + 2, T + 1>(ans+i,prA,u);
    }
  // if (ans>=pr) { ans=z-pr; }
  // else         { z=ans;    }
  if (mpn_cmp(ans+T,prA,T+1)>=0)
     { mpn_sub_fixed_n<T>(z,ans+T,prA); }
  else
     { inline_mpn_copyi<T>(z,ans+T); }
#else
  Mont_Mult(z, x, y, t);
#endif
}

inline void Zp_Data::Mont_Mult(mp_limb_t* z,const mp_limb_t* x,const mp_limb_t* y) const
{
  if (not cpu_has_bmi2())
    return Mont_Mult_variable(z, x, y);
#ifdef __BMI2__
  return Mont_Mult_switch(z, x, y);
#else
  return Mont_Mult_variable(z, x, y);
#endif
}

inline void Zp_Data::Mont_Mult_max(mp_limb_t* z, const mp_limb_t* x,
    const mp_limb_t* y, int max_t) const
{
  assert(t <= max_t);
  Mont_Mult(z, x, y);
}

#endif
