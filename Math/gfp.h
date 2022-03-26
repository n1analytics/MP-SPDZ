#ifndef _gfp
#define _gfp

#include <iostream>
using namespace std;

#include "Math/gf2n.h"
#include "Math/modp.h"
#include "Math/Zp_Data.h"
#include "Math/field_types.h"
#include "Math/Bit.h"
#include "Math/Setup.h"
#include "Tools/random.h"
#include "Processor/OnlineOptions.h"

#include "Math/modp.hpp"

/* This is a wrapper class for the modp data type
 * It is used to be interface compatible with the gfp
 * type, which then allows us to template the Share
 * data type.
 *
 * So gfp is used ONLY for the stuff in the finite fields
 * we are going to be doing MPC over, not the modp stuff
 * for the FHE scheme
 */

template<class T> class Input;
template<class T> class SPDZ;
template<class T> class Square;
class FFT_Data;

template<class T> void generate_prime_setup(string, int, int);

#ifndef GFP_MOD_SZ
#define GFP_MOD_SZ 2
#endif

#if GFP_MOD_SZ > MAX_MOD_SZ
#error GFP_MOD_SZ must be at most MAX_MOD_SZ
#endif

/**
 * Type for values in a field defined by integers modulo a prime
 * in a specific range for fixed storage.
 * It supports basic arithmetic operations and bit-wise operations.
 * The latter use the canonical representation in the range `[0, p-1]`.
 * ``X`` is a counter to allow several moduli being used at the same time.
 * ``L`` is the number of 64-bit limbs, that is,
 * the prime has to have bit length in `[64*L-63, 64*L]`.
 * See ``gfpvar_`` for a more flexible alternative.
 */
template<int X, int L>
class gfp_ : public ValueInterface
{
  typedef modp_<L> modp_type;    // 64bit的模数分支

  modp_type a;
  static Zp_Data ZpD;

  static thread_local vector<gfp_> powers;

  public:

  typedef gfp_ value_type;
  typedef gfp_ Scalar;

  typedef gfp_<X + 1, L> next;
  typedef ::Square<gfp_> Square;

  typedef FFT_Data FD;

  static const int N_LIMBS = L;
  static const int MAX_N_BITS = 64 * L;
  static const int N_BYTES = sizeof(a);

  // must be negative
  static const int N_BITS = -1;

  static const int MAX_EDABITS = MAX_N_BITS;

  template<class T>
  static void init(bool mont = true)
    { init_field(T::pr(), mont); }
  /**
   * Initialize the field.
   * @param p: prime modulus
   * @param mont: whether to use Montgomery representation
   */
  static void init_field(const bigint& p,bool mont=true);
  /**
   * Initialize the field to a prime of a given bit length.
   * @param lgp: bit length
   * @param mont: whether to use Montgomery representation
   */
  static void init_default(int lgp, bool mont = true);
  static void read_or_generate_setup(string dir, const OnlineOptions& opts);
  template<class T>
  static void generate_setup(string dir, int nplayers, int lgp)
    { generate_prime_setup<T>(dir, nplayers, lgp); }
  template<class T>
  static void write_setup(int nplayers)
    { write_setup(get_prep_sub_dir<T>(nplayers)); }
  static void write_setup(string dir)
    { write_online_setup(dir, pr()); }
  static void check_setup(string dir);

  /**
   * Get the prime modulus
   */
  static const bigint& pr()
    { return ZpD.pr; }
  static int t()
    { return L;  }
  static Zp_Data& get_ZpD()
    { return ZpD; }

  static DataFieldType field_type() { return DATA_INT; }
  static char type_char() { return 'p'; }
  static string type_short() { return "p"; }
  static string type_string() { return "gfp"; }

  static int size() { return t() * sizeof(mp_limb_t); }
  static int size_in_bits() { return 8 * size(); }
  static int length() { return ZpD.pr_bit_length; }
  static int n_bits() { return length() - 1; }

  static void reqbl(int n);

  static bool allows(Dtype type);

  static void specification(octetStream& os);

  static const true_type invertible;
  static const true_type prime_field;

  static gfp_ Mul(gfp_ a, gfp_ b) { return a * b; }

  static gfp_ power_of_two(bool bit, int exp);

  void assign_zero()        { assignZero(a,ZpD); }
  void assign_one()         { assignOne(a,ZpD); } 
  void assign(const void* buffer) { a.assign(buffer, ZpD.get_t()); }

  modp_type get() const           { return a; }

  unsigned long debug() const { return a.get_limb(0); }

  const void* get_ptr() const { return &a.x; }
  void* get_ptr()             { return &a.x; }

  /**
   * Initialize to zero.
   */
  gfp_()              { assignZero(a,ZpD); }
  template<int LL>
  gfp_(const modp_<LL>& g) { a=g; }
  /**
   * Convert from integer without range restrictions.
   */
  gfp_(const mpz_class& x) { to_modp(a, x, ZpD); }
  gfp_(int x) : gfp_(long(x)) {}
  gfp_(long x);
  gfp_(word x) : gfp_(bigint::tmp = x) {}
  template<class T>
  gfp_(IntBase<T> x) : gfp_(x.get()) {}
  /**
   * Convert from different domain via canonical integer representation.
   */
  template<int Y>
  gfp_(const gfp_<Y, L>& x);
  gfp_(const gfpvar& other);
  template<int K>
  gfp_(const SignedZ2<K>& other);

  void zero_overhang();
  void check();

  bool is_zero() const            { return isZero(a,ZpD); }
  bool is_one()  const            { return isOne(a,ZpD); }
  bool is_bit()  const            { return is_zero() or is_one(); }
  bool equal(const gfp_& y) const  { return areEqual(a,y.a,ZpD); }
  bool operator==(const gfp_& y) const { return equal(y); }
  bool operator!=(const gfp_& y) const { return !equal(y); }

  // x+y
  void add(octetStream& os)
    { add(os.consume(size())); }
  void add(const gfp_& x,const gfp_& y)
    { ZpD.Add<L>(a.x,x.a.x,y.a.x); }
  void add(void* x)
    { ZpD.Add<L>(a.x,a.x,(mp_limb_t*)x); }
  void sub(const gfp_& x,const gfp_& y)
    { ZpD.Sub<L>(a.x,x.a.x,y.a.x); }
  // = x * y
  void mul(const gfp_& x,const gfp_& y)
    { a.template mul<L>(x.a,y.a,ZpD); }

  gfp_ lazy_add(const gfp_& x) const { return *this + x; }
  gfp_ lazy_mul(const gfp_& x) const { return *this * x; }

  gfp_ operator+(const gfp_& x) const { gfp_ res; res.add(*this, x); return res; }
  gfp_ operator-(const gfp_& x) const { gfp_ res; res.sub(*this, x); return res; }
  gfp_ operator*(const gfp_& x) const { gfp_ res; res.mul(*this, x); return res; }
  gfp_ operator/(const gfp_& x) const { return *this * x.invert(); }
  gfp_& operator+=(const gfp_& x) { add(*this, x); return *this; }
  gfp_& operator-=(const gfp_& x) { sub(*this, x); return *this; }
  gfp_& operator*=(const gfp_& x) { mul(*this, x); return *this; }

  gfp_ operator-() { gfp_ res = *this; res.negate(); return res; }

  gfp_ invert() const;
  void negate() 
    { Negate(a,a,ZpD); }

  /**
   * Deterministic square root.
   */
  gfp_ sqrRoot();

  /**
   * Sample with uniform distribution.
   * @param G randomness generator
   * @param n (unused)
   */
  void randomize(PRNG& G, int n = -1)
    { (void) n; a.randomize(G,ZpD); }
  // faster randomization, see implementation for explanation
  void almost_randomize(PRNG& G);

  void output(ostream& s,bool human) const
    { a.output(s,ZpD,human); }
  void input(istream& s,bool human)
    { a.input(s,ZpD,human); }

  /**
   * Human-readable output in the range `[-p/2, p/2]`.
   * @param s output stream
   * @param x value
   */
  friend ostream& operator<<(ostream& s,const gfp_& x)
    { x.output(s,true);
      return s;
    }
  /**
   * Human-readable input without range restrictions
   * @param s input stream
   * @param x value
   */
  friend istream& operator>>(istream& s,gfp_& x)
    { x.input(s,true);
      return s;
    }

  /* Bitwise Ops 
   *   - Converts gfp args to bigints and then converts answer back to gfp
   */
  gfp_ operator&(const gfp_& x) { return (bigint::tmp = *this) &= bigint(x); }
  gfp_ operator^(const gfp_& x) { return (bigint::tmp = *this) ^= bigint(x); }
  gfp_ operator|(const gfp_& x) { return (bigint::tmp = *this) |= bigint(x); }
  gfp_ operator<<(int i) const;
  gfp_ operator>>(int i) const;
  gfp_ operator<<(const gfp_& i) const;
  gfp_ operator>>(const gfp_& i) const;

  gfp_& operator&=(const gfp_& x) { *this = *this & x; return *this; }
  gfp_& operator<<=(int i) { *this << i; return *this; }
  gfp_& operator>>=(int i) { *this >> i; return *this; }

  void force_to_bit() { throw runtime_error("impossible"); }

  /**
   * Append to buffer in native format.
   * @param o buffer
   * @param n (unused)
   */
  void pack(octetStream& o, int n = -1) const
    { (void) n; a.pack(o); }
  /**
   * Read from buffer in native format
   * @param o buffer
   * @param n (unused)
   */
  void unpack(octetStream& o, int n = -1)
    { (void) n; a.unpack(o); }

  void convert_destroy(bigint& x) { a.convert_destroy(x, ZpD); }

  void to(bigint& res) const
  {
    res = *this;
  }

  // Convert representation to and from a bigint number
  friend void to_bigint(bigint& ans,const gfp_& x,bool reduce=true)
    { x.a.template to_bigint<L>(ans, x.ZpD, reduce); }
  friend void to_gfp(gfp_& ans,const bigint& x)
    { to_modp(ans.a,x,ans.ZpD); }
};

typedef gfp_<0, GFP_MOD_SZ> gfp0;
typedef gfp_<1, GFP_MOD_SZ> gfp1;

template<int X, int L>
Zp_Data gfp_<X, L>::ZpD;

template<int X, int L>
thread_local vector<gfp_<X, L>> gfp_<X, L>::powers;

template<int X, int L>
gfp_<X, L>::gfp_(long x)
{
  if (x == 0)
    assign_zero();
  else
    *this = bigint::tmp = x;
}

template<int X, int L>
template<int Y>
gfp_<X, L>::gfp_(const gfp_<Y, L>& x)
{
  to_bigint(bigint::tmp, x);
  *this = bigint::tmp;
}

template<int X, int L>
template<int K>
gfp_<X, L>::gfp_(const SignedZ2<K>& other)
{
  if (K >= ZpD.pr_bit_length)
    *this = bigint::tmp = other;
  else
    a.convert(abs(other).get(), other.size_in_limbs(), ZpD, other.negative());
}

template <int X, int L>
inline void gfp_<X, L>::zero_overhang()
{
  a.x[t() - 1] &= ZpD.overhang_mask();
}

template<class T>
void to_signed_bigint(bigint& ans, const T& x)
{
    ans = x;
    // get sign and abs(x)
    if (ans > T::get_ZpD().pr_half)
        ans -= T::pr();
}

#endif
