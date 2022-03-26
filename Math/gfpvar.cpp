/*
 * gfpvar_<X, L>.cpp
 *
 */

#include "gfpvar.h"
#include "Setup.h"
#include "Protocols/Share.h"

#include "gfp.hpp"

template<int X, int L>
Zp_Data gfpvar_<X, L>::ZpD;

template<int X, int L>
string gfpvar_<X, L>::type_string()
{
    return "gfp";
}

template<int X, int L>
string gfpvar_<X, L>::type_short()
{
    return "p";
}

template<int X, int L>
char gfpvar_<X, L>::type_char()
{
    return 'p';
}

template<int X, int L>
void gfpvar_<X, L>::specification(octetStream& os)
{
    os.store(pr());
}

template<int X, int L>
int gfpvar_<X, L>::length()
{
    return ZpD.pr_bit_length;
}

template<int X, int L>
int gfpvar_<X, L>::size()
{
    return ZpD.get_t() * sizeof(mp_limb_t);
}

template<int X, int L>
int gfpvar_<X, L>::size_in_bits()
{
    return size() * 8;
}

template<int X, int L>
bool gfpvar_<X, L>::allows(Dtype dtype)
{
    return gfp_<0, 0>::allows(dtype);
}

template<int X, int L>
DataFieldType gfpvar_<X, L>::field_type()
{
    return gfp_<0, 0>::field_type();
}

template<int X, int L>
void gfpvar_<X, L>::init_field(bigint prime, bool montgomery)
{
    // 给素数域初始化素数 并确定是否使用montgomery模乘
    ZpD.init(prime, montgomery);
    if (ZpD.get_t() > N_LIMBS)
        throw wrong_gfp_size("gfpvar_<X, L>", prime, "MAX_MOD_SZ", ZpD.get_t() * 2);
}

template<int X, int L>
void gfpvar_<X, L>::init_default(int lgp, bool montgomery)
{
    init_field(SPDZ_Data_Setup_Primes(lgp), montgomery);
}

template<int X, int L>
const Zp_Data& gfpvar_<X, L>::get_ZpD()
{
    return ZpD;
}

template<int X, int L>
bigint& gfpvar_<X, L>::pr()
{
    return ZpD.pr;
}

template<int X, int L>
void gfpvar_<X, L>::check_setup(string dir)
{
    ::check_setup(dir, pr());
}

template<int X, int L>
void gfpvar_<X, L>::write_setup(string dir)
{
    write_online_setup(dir, pr());
}

template<int X, int L>
gfpvar_<X, L>::gfpvar_()
{
}

template<int X, int L>
gfpvar_<X, L>::gfpvar_(int other)
{
    to_modp(a, other, ZpD);
}

template<int X, int L>
gfpvar_<X, L>::gfpvar_(const bigint& other)
{
    to_modp(a, other, ZpD);
}

template<int X, int L>
gfpvar_<X, L>::gfpvar_(int128 other) :
        gfpvar_(
                (bigint::tmp = other.get_lower()
                        + ((bigint::tmp2 = other.get_upper()) << 64)))
{
}

template<int X, int L>
gfpvar_<X, L>::gfpvar_(BitVec_<long> other) :
        gfpvar_(bigint::tmp = other.get())
{
}

template<int X, int L>
void gfpvar_<X, L>::assign(const void* buffer)
{
    a.assign(buffer, ZpD.get_t());
}

template<int X, int L>
void gfpvar_<X, L>::assign_zero()
{
    *this = {};
}

template<int X, int L>
void gfpvar_<X, L>::assign_one()
{
    assignOne(a, ZpD);
}

template<int X, int L>
bool gfpvar_<X, L>::is_zero()
{
    return isZero(a, ZpD);
}

template<int X, int L>
bool gfpvar_<X, L>::is_one()
{
    return isOne(a, ZpD);
}

template<int X, int L>
bool gfpvar_<X, L>::is_bit()
{
    return is_zero() or is_one();
}

template<int X, int L>
typename gfpvar_<X, L>::modp_type gfpvar_<X, L>::get() const
{
    return a;
}

template<int X, int L>
const void* gfpvar_<X, L>::get_ptr() const
{
    return a.get();
}

template<int X, int L>
void* gfpvar_<X, L>::get_ptr()
{
    return &a;
}

template<int X, int L>
void gfpvar_<X, L>::zero_overhang()
{
    a.zero_overhang(ZpD);
}

template <int X, int L>
void gfpvar_<X, L>::check()
{
    assert(mpn_cmp(a.get(), ZpD.get_prA(), ZpD.get_t()) < 0);
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::operator +(const gfpvar_<X, L>& other) const
{
    gfpvar_<X, L> res;
    Add(res.a, a, other.a, ZpD);
    return res;
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::operator -(const gfpvar_<X, L>& other) const
{
    gfpvar_<X, L> res;
    Sub(res.a, a, other.a, ZpD);
    return res;
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::operator *(const gfpvar_<X, L>& other) const
{
    gfpvar_<X, L> res;
    Mul(res.a, a, other.a, ZpD);
    return res;
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::operator /(const gfpvar_<X, L>& other) const
{
    return *this * other.invert();
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::operator <<(int other) const
{
    return bigint::tmp = (bigint::tmp = *this) << other;
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::operator >>(int other) const
{
    return bigint::tmp = (bigint::tmp = *this) >> other;
}

template<int X, int L>
gfpvar_<X, L>& gfpvar_<X, L>::operator +=(const gfpvar_<X, L>& other)
{
    Add(a, a, other.a, ZpD);
    return *this;
}

template<int X, int L>
gfpvar_<X, L>& gfpvar_<X, L>::operator -=(const gfpvar_<X, L>& other)
{
    Sub(a, a, other.a, ZpD);
    return *this;
}

template<int X, int L>
gfpvar_<X, L>& gfpvar_<X, L>::operator *=(const gfpvar_<X, L>& other)
{
    Mul(a, a, other.a, ZpD);
    return *this;
}

template<int X, int L>
gfpvar_<X, L>& gfpvar_<X, L>::operator &=(const gfpvar_<X, L>& other)
{
    *this = bigint::tmp = (bigint::tmp = *this) & (bigint::tmp2 = other);
    return *this;
}

template<int X, int L>
gfpvar_<X, L>& gfpvar_<X, L>::operator >>=(int other)
{
    return *this = *this >> other;
}

template<int X, int L>
bool gfpvar_<X, L>::operator ==(const gfpvar_<X, L>& other) const
{
    return areEqual(a, other.a, ZpD);
}

template<int X, int L>
bool gfpvar_<X, L>::operator !=(const gfpvar_<X, L>& other) const
{
    return not (*this == other);
}

template<int X, int L>
void gfpvar_<X, L>::add(octetStream& other)
{
    *this += other.get<gfpvar_<X, L>>();
}

template<int X, int L>
void gfpvar_<X, L>::negate()
{
    *this = gfpvar_<X, L>() - *this;
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::invert() const
{
    gfpvar_<X, L> res;
    Inv(res.a, a, ZpD);
    return res;
}

template<int X, int L>
gfpvar_<X, L> gfpvar_<X, L>::sqrRoot() const
{
    bigint ti = *this;
    ti = sqrRootMod(ti, ZpD.pr);
    if (!isOdd(ti))
        ti = ZpD.pr - ti;
    return ti;
}

template<int X, int L>
void gfpvar_<X, L>::randomize(PRNG& G, int)
{
    a.randomize(G, ZpD);
}

template<int X, int L>
void gfpvar_<X, L>::almost_randomize(PRNG& G)
{
    randomize(G);
}

template<int X, int L>
void gfpvar_<X, L>::pack(octetStream& os, int) const
{
    a.pack(os, ZpD);
}

template<int X, int L>
void gfpvar_<X, L>::unpack(octetStream& os, int)
{
    a.unpack(os, ZpD);
}

template<int X, int L>
void gfpvar_<X, L>::output(ostream& o, bool human) const
{
    a.output(o, ZpD, human);
}

template<int X, int L>
void gfpvar_<X, L>::input(istream& i, bool human)
{
    a.input(i, ZpD, human);
}

template class gfpvar_<0, MAX_MOD_SZ / 2>;
template class gfpvar_<1, MAX_MOD_SZ>;
template class gfpvar_<2, MAX_MOD_SZ>;
