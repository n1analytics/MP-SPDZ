#ifndef _Rq_Element
#define _Rq_Element

/* An Rq Element is something held modulo Q_0 = p0 or Q_1 = p0*p1
 *
 * The level is the value of Q_level which is being used.
 * Elements can be held in either representation and one can switch
 * representations at will.
 *   - Although in the evaluation we do not multiply at level 1
 *     we do need to multiply at level 1 for KeyGen and Encryption.
 *
 * Usually we keep level 0 in evaluation and level 1 in polynomial
 * representation though
 */

#include "FHE/Ring_Element.h"
#include "FHE/FHE_Params.h"
#include "FHE/tools.h"
#include "FHE/Generator.h"
#include "Plaintext.h"
#include <vector>

// Forward declare the friend functions
class Rq_Element;
void add(Rq_Element& ans,const Rq_Element& a,const Rq_Element& b);
void sub(Rq_Element& ans,const Rq_Element& a,const Rq_Element& b);
void mul(Rq_Element& ans,const Rq_Element& a,const Rq_Element& b);
void mul(Rq_Element& ans,const Rq_Element& a,const bigint& b);

class Rq_Element
{
protected:


  public:

  vector<Ring_Element> a;
  int lev;
  int n_mults() const { return a.size() - 1; }

  void change_rep(RepType r);
  void change_rep(RepType r0,RepType r1);

  void set_data(const vector<FFT_Data>& prd);
  void assign_zero(const vector<FFT_Data>& prd);
  void assign_zero(); 
  void assign_one(); 
  void partial_assign(const Rq_Element& e);

  // Must be careful not to call by mistake
  Rq_Element(RepType r0=evaluation,RepType r1=polynomial) :
    a({r0, r1}), lev(n_mults()) {}

  // Pass in a pair of FFT_Data as a vector
  Rq_Element(const vector<FFT_Data>& prd, RepType r0 = evaluation,
      RepType r1 = polynomial);

  Rq_Element(const FHE_Params& params, RepType r0, RepType r1) :
      Rq_Element(params.FFTD(), r0, r1) {}

  Rq_Element(const FHE_Params& params) :
      Rq_Element(params.FFTD()) {}

  Rq_Element(const FHE_PK& pk);

  Rq_Element(const Ring_Element& b0,const Ring_Element& b1) :
    a({b0, b1}), lev(n_mults()) {}

  Rq_Element(const Ring_Element& b0) :
    a({b0}), lev(n_mults()) {}

  template<class T, class FD, class S>
  Rq_Element(const FHE_Params& params, const Plaintext<T, FD, S>& plaintext) :
      Rq_Element(params)
  {
    from(plaintext.get_iterator());
  }

  template<class U, class V>
  Rq_Element(const vector<FFT_Data>& prd, const vector<U>& b0,
      const vector<V>& b1, RepType r = evaluation) :
      Rq_Element(prd, r, r)
  {
    a[0] = Ring_Element(prd[0], r, b0);
    a[1] = Ring_Element(prd[1], r, b1);
  }

  const Ring_Element& get(int i) const { return a[i]; }

  /* Functional Operators */
  void negate();
  friend void add(Rq_Element& ans,const Rq_Element& a,const Rq_Element& b);
  friend void sub(Rq_Element& ans,const Rq_Element& a,const Rq_Element& b);
  friend void mul(Rq_Element& ans,const Rq_Element& a,const Rq_Element& b);
  friend void mul(Rq_Element& ans,const Rq_Element& a,const bigint& b);

  void add(octetStream& os);

  template<class S>
  Rq_Element& operator+=(const vector<S>& other);

  Rq_Element& operator+=(const Rq_Element& other) { ::add(*this, *this, other); return *this; }

  Rq_Element operator+(const Rq_Element& b) const { Rq_Element res(*this); ::add(res, *this, b); return res; }
  Rq_Element operator-(const Rq_Element& b) const { Rq_Element res(*this); sub(res, *this, b); return res; }
  template <class T>
  Rq_Element operator*(const T& b) const { Rq_Element res(*this); mul(res, *this, b); return res; }

  // Multiply something by p1 and make level 1
  void mul_by_p1();

  Rq_Element mul_by_X_i(int i) const;

  void randomize(PRNG& G,int lev=-1);

  // Scale from level 1 to level 0, if at level 0 do nothing
  void Scale(const bigint& p);

  bool equals(const Rq_Element& a) const;
  bool operator==(const Rq_Element& a) const { return equals(a); }
  bool operator!=(const Rq_Element& a) const { return !equals(a); }

  int level() const { return lev; }
  void lower_level() { if (lev==1) { lev=0; }  }
  // raise_level boosts a level 0 to a level 1 (or does nothing if level =1)
  void raise_level();
  void check_level() const;
  void set_level(int level) { lev = (level == -1 ? n_mults() : level); }
  void partial_assign(const Rq_Element& a, const Rq_Element& b);

  // Converting to and from a vector of bigint's Again I/O is in poly rep
  vector<bigint>  to_vec_bigint() const;
  void to_vec_bigint(vector<bigint>& v) const;

  ConversionIterator get_iterator() const;
  template <class T>
  void from(const Generator<T>& generator, int level=-1);

  template <class T>
  void from(const vector<T>& source, int level=-1)
    {
      from(Iterator<T>(source), level);
    }

  bigint infinity_norm() const;

  bigint get_prime(int i) const
    { return a[i].get_prime(); }

  bigint get_modulus() const
    { bigint ans=1;
      for(int i=0; i<=lev; ++i) ans*=a[i].get_prime();
      return ans;
    }

  /* Pack and unpack into an octetStream 
   *   For unpack we assume the prData for a0 and a1 has been assigned 
   *   correctly already
   */
  void pack(octetStream& o) const;
  void unpack(octetStream& o);

  void output(ostream& s) const;
  void input(istream& s);

  void check(const FHE_Params& params) const;

  size_t report_size(ReportType type) const;

  void print_first_non_zero() const;
};
   
template<int L>
inline void mul(Rq_Element& ans,const bigint& a,const Rq_Element& b)
{ mul(ans,b,a); }

template<class S>
Rq_Element& Rq_Element::operator+=(const vector<S>& other)
{
  Rq_Element tmp = *this;
  tmp.from(Iterator<S>(other), lev);
  ::add(*this, *this, tmp);
  return *this;
}

template <class T>
void Rq_Element::from(const Generator<T>& generator, int level)
{
  set_level(level);
  if (lev == 1)
    {
      auto clone = generator.clone();
      a[1].from(*clone);
      delete clone;
    }
  a[0].from(generator);
}

#endif
