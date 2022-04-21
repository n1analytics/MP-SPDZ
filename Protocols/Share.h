
#ifndef _Share
#define _Share

/* Class for holding a share of either a T or gfp element */ 

#include <vector>
#include <iostream>
using namespace std;

#include "Math/gf2n.h"
#include "Protocols/SPDZ.h"
#include "Protocols/SemiShare.h"
#include "ShareInterface.h"

// Forward declaration as apparently this is needed for friends in templates
template<class T> class Share;

template<class T> class MAC_Check_;
template<class T> class Direct_MAC_Check;
template<class T> class MascotMultiplier;
template<class T> class MascotFieldPrep;
template<class T> class MascotTripleGenerator;
template<class T> class MascotPrep;
template<class T> class MascotTriplePrep;

union square128;

namespace GC
{
template<class T> class TinierSecret;
}

// abstracting SPDZ and SPDZ-wise
template<class T, class V>
class Share_ : public ShareInterface
{
   T a;        // The share
   V mac;      // Shares of the mac

   public:

   typedef GC::NoShare part_type;
   typedef V mac_key_type;
   typedef V mac_type;
   typedef T share_type;
   typedef V mac_share_type;
   typedef typename T::open_type open_type;
   typedef typename T::clear clear;

#ifndef NO_MIXED_CIRCUITS
   typedef GC::TinierSecret<gf2n_short> bit_type;
#endif

   const static bool needs_ot = T::needs_ot;
   const static bool dishonest_majority = T::dishonest_majority;
   const static bool variable_players = T::variable_players;
   const static bool has_mac = true;

   static int size()
     { return T::size() + V::size(); }

   static char type_char()
     { return T::type_char(); }

   static int threshold(int nplayers)
     { return T::threshold(nplayers); }

   template<class U>
   static void read_or_generate_mac_key(string directory, const Player& P,
           U& key);

   static void specification(octetStream& os)
     { T::specification(os); }

   static Share_ constant(const clear& aa, int my_num, const typename V::Scalar& alphai)
     { return Share_(aa, my_num, alphai); }

   template<class U, class W>
   void assign(const Share_<U, W>& S)
     { a=S.get_share(); mac=S.get_mac(); }
   void assign(const char* buffer)
     { a.assign(buffer); mac.assign(buffer + T::size()); }
   void assign_zero()
     { a.assign_zero(); 
       mac.assign_zero(); 
     }
   void assign(const clear& aa, int my_num, const typename V::Scalar& alphai);

   Share_()                   { assign_zero(); }
   template<class U, class W>
   Share_(const Share_<U, W>& S) { assign(S); }
   Share_(const clear& aa, int my_num, const typename V::Scalar& alphai)
     { assign(aa, my_num, alphai); }
   Share_(const T& share, const V& mac) : a(share), mac(mac) {}

   const T& get_share() const          { return a; }
   const V& get_mac() const            { return mac; }
   void set_share(const T& aa)  { a=aa; }
   void set_mac(const V& aa)    { mac=aa; }

   /* Arithmetic Routines */
   void mul(const Share_<T, V>& S,const clear& aa);
   void add(const Share_<T, V>& S1,const Share_<T, V>& S2);
   void sub(const Share_<T, V>& S1,const Share_<T, V>& S2);

   Share_<T, V> operator+(const Share_<T, V>& x) const
   { Share_<T, V> res; res.add(*this, x); return res; }
   Share_<T, V> operator-(const Share_<T, V>& x) const
   { Share_<T, V> res; res.sub(*this, x); return res; }
   template <class U>
   Share_<T, V> operator*(const U& x) const
   { Share_<T, V> res; res.mul(*this, x); return res; }
   Share_<T, V> operator/(const T& x) const
   { Share_<T, V> res; res.set_share(a / x); res.set_mac(mac / x); return res; }

   Share_<T, V>& operator+=(const Share_<T, V>& x) { add(*this, x); return *this; }
   Share_<T, V>& operator-=(const Share_<T, V>& x) { sub(*this, x); return *this; }
   template <class U>
   Share_<T, V>& operator*=(const U& x) { mul(*this, x); return *this; }
   template <class U>
   Share_<T, V>& operator/=(const U& x) { *this = *this / x; return *this; }

   Share_<T, V> operator<<(int i) { return this->operator*(clear(1) << i); }
   Share_<T, V>& operator<<=(int i) { return *this = *this << i; }

   Share_<T, V> operator>>(int i) const { return {a >> i, mac >> i}; }

   void force_to_bit() { a.force_to_bit(); }

   // Input and output from a stream
   //  - Can do in human or machine only format (later should be faster)
   void output(ostream& s,bool human) const
     { a.output(s,human);     if (human) { s << " "; }
       mac.output(s,human);
     }
   void input(istream& s,bool human)
     { a.input(s,human);
       mac.input(s,human);
     }

   friend ostream& operator<<(ostream& s, const Share_<T, V>& x) { x.output(s, true); return s; }

   void pack(octetStream& os, bool full = true) const;
   void unpack(octetStream& os, bool full = true);
};

// SPDZ(2k) only
template<class T>
class Share : public Share_<SemiShare<T>, SemiShare<T>>
{
public:
    typedef Share_<SemiShare<T>, SemiShare<T>> super;

    typedef typename T::Scalar mac_key_type;
    typedef T mac_type;

    typedef Share<typename T::next> prep_type;
    typedef Share input_check_type;
    typedef Share input_type;
    typedef MascotMultiplier<Share> Multiplier;
    typedef MascotTripleGenerator<prep_type> TripleGenerator;
    typedef T sacri_type;
    typedef typename T::Square Rectangle;
    typedef Rectangle Square;

    typedef MAC_Check_<Share> MAC_Check;
    typedef Direct_MAC_Check<Share> Direct_MC;
    typedef ::Input<Share> Input;
    typedef ::PrivateOutput<Share> PrivateOutput;
    typedef SPDZ<Share> Protocol;
    typedef MascotFieldPrep<Share> LivePrep;
    typedef MascotPrep<Share> RandomPrep;
    typedef MascotTriplePrep<Share> TriplePrep;

    static const bool expensive = true;

    static string type_short()
      { return string(1, T::type_char()); }

    static string type_string()
      { return "SPDZ " + T::type_string(); }

    Share() {}
    template<class U>
    Share(const U& other) : super(other) {}
    Share(const SemiShare<T>& share, const SemiShare<T>& mac) :
            super(share, mac) {}
};

template<class T>
class ArithmeticOnlyMascotShare : public Share<T>
{
    typedef ArithmeticOnlyMascotShare This;
    typedef Share<T> super;

public:
    typedef GC::NoShare bit_type;

    typedef MAC_Check_<This> MAC_Check;
    typedef ::Input<This> Input;
    typedef ::PrivateOutput<This> PrivateOutput;
    typedef SPDZ<This> Protocol;

    ArithmeticOnlyMascotShare() {}
    template<class U>
    ArithmeticOnlyMascotShare(const U& other) : super(other) {}
    ArithmeticOnlyMascotShare(const SemiShare<T>& share,
            const SemiShare<T>& mac) :
            super(share, mac) {}
};

template <class T, class V>
Share_<T, V> operator*(const typename T::clear& y, const Share_<T, V>& x) { Share_<T, V> res; res.mul(x, y); return res; }

template<class T, class V>
inline void Share_<T, V>::add(const Share_<T, V>& S1,const Share_<T, V>& S2)
{
  a = (S1.a + S2.a);
  mac = (S1.mac + S2.mac);
}

template<class T, class V>
void Share_<T, V>::sub(const Share_<T, V>& S1,const Share_<T, V>& S2)
{
  a = (S1.a - S2.a);
  mac = (S1.mac - S2.mac);
}

template<class T, class V>
inline void Share_<T, V>::mul(const Share_<T, V>& S,const clear& aa)
{
  a = S.a * aa;
  mac = aa * S.mac;
}

template<class T, class V>
inline void Share_<T, V>::assign(const clear& aa, int my_num,
    const typename V::Scalar& alphai)
{
  a = T::constant(aa, my_num);
  mac = aa * alphai;
#ifdef DEBUG_MAC
  cout << "load " << hex << mac << " = " << aa << " * " << alphai << endl;
#endif
}

#endif
