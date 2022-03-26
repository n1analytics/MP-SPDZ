/*
 * vargfp.h
 *
 */

#ifndef MATH_GFPVAR_H_
#define MATH_GFPVAR_H_

#include "modp.h"
#include "Zp_Data.h"
#include "Setup.h"
#include "Square.h"

class FFT_Data;
template<class T> class BitVec_;

/**
 * Type for values in a field defined by integers modulo a prime
 * up to a certain length for fixed storage.
 * ``X`` is a counter to allow several moduli being used at the same time.
 * ``L`` is the maximum number of 64-bit limbs, that is,
 * the prime has to have bit length at most `64*L`.
 * The interface replicates ``gfp_``.
 */
template<int X, int L>
class gfpvar_
{
    typedef modp_<L> modp_type;

    static Zp_Data ZpD;

    modp_type a;

public:
    typedef gfpvar_ Scalar;
    typedef FFT_Data FD;

    typedef ::Square<gfpvar_> Square;
    typedef gfpvar_ next;
    typedef gfpvar_ value_type;

    static const int MAX_N_BITS = modp_type::MAX_N_BITS;
    static const int MAX_EDABITS = modp_type::MAX_N_BITS;
    static const int N_LIMBS = modp_type::N_LIMBS;
    static const int N_BITS = -1;

    static const true_type invertible;
    static const true_type prime_field;
    static const false_type characteristic_two;

    static string type_string();
    static string type_short();
    static char type_char();

    static void specification(octetStream& os);

    static int length();
    static int size();
    static int size_in_bits();

    static bool allows(Dtype dtype);
    static DataFieldType field_type();

    static void init_field(bigint prime, bool montgomery = true);
    static void init_default(int lgp, bool montgomery = true);
    template<class T>
    static void init(bool montgomery)
    {
        init_field(T::pr(), montgomery);
    }

    static const Zp_Data& get_ZpD();
    static bigint& pr();

    template<class T>
    static void generate_setup(string prep_data_prefix, int nplayers, int lgp);
    static void check_setup(string dir);
    static void write_setup(string dir);
    template<class T>
    static void write_setup(int nplayers)
    {
        write_setup(get_prep_sub_dir<T>(nplayers));
    }

    gfpvar_();
    gfpvar_(int other);
    gfpvar_(int128 other);
    gfpvar_(BitVec_<long> other);
    gfpvar_(const bigint& other);

    template<int M>
    gfpvar_(const modp_<M>& other, const Zp_Data& ZpD)
    {
        if (get_ZpD() == ZpD)
            a = other;
        else
        {
            to_bigint(bigint::tmp, other, ZpD);
            *this = bigint::tmp;
        }
    }

    template<int XX, int LL>
    gfpvar_(const gfp_<XX, LL>& other)
    {
        assert(pr() == other.pr());
        a = other.get();
    }

    void assign(const void* buffer);

    void assign_zero();
    void assign_one();

    bool is_zero();
    bool is_one();
    bool is_bit();

    modp_type get() const;
    const void* get_ptr() const;
    void* get_ptr();

    void zero_overhang();
    void check();

    gfpvar_ operator+(const gfpvar_& other) const;
    gfpvar_ operator-(const gfpvar_& other) const;
    gfpvar_ operator*(const gfpvar_& other) const;
    gfpvar_ operator/(const gfpvar_& other) const;

    gfpvar_ operator<<(int other) const;
    gfpvar_ operator>>(int other) const;

    gfpvar_& operator+=(const gfpvar_& other);
    gfpvar_& operator-=(const gfpvar_& other);
    gfpvar_& operator*=(const gfpvar_& other);
    gfpvar_& operator&=(const gfpvar_& other);

    gfpvar_& operator>>=(int other);

    bool operator==(const gfpvar_& other) const;
    bool operator!=(const gfpvar_& other) const;

    void add(octetStream& other);

    void negate();

    gfpvar_ invert() const;

    gfpvar_ sqrRoot() const;

    void randomize(PRNG& G, int n_bits = -1);
    void almost_randomize(PRNG& G);

    void pack(octetStream& os, int n_bits = -1) const;
    void unpack(octetStream& os, int n_bits = -1);

    void output(ostream& o, bool human) const;
    void input(istream& o, bool human);
};

typedef gfpvar_<0, MAX_MOD_SZ / 2> gfpvar;
typedef gfpvar_<1, MAX_MOD_SZ> gfpvar1;
typedef gfpvar_<2, MAX_MOD_SZ> gfpvar2;

typedef gfpvar gfp;

template<int X, int L>
const true_type gfpvar_<X, L>::invertible;
template<int X, int L>
const true_type gfpvar_<X, L>::prime_field;
template<int X, int L>
const false_type gfpvar_<X, L>::characteristic_two;

template<int X, int L>
template<class T>
void gfpvar_<X, L>::generate_setup(string prep_data_prefix,
    int nplayers, int lgp)
{
    generate_prime_setup<T>(prep_data_prefix, nplayers, lgp);
}

template<int X, int L>
ostream& operator <<(ostream& o, const gfpvar_<X, L>& x)
{
    x.output(o, true);
    return o;
}

template<int X, int L>
istream& operator >>(istream& i, gfpvar_<X, L>& x)
{
    x.input(i, true);
    return i;
}

#endif /* MATH_GFPVAR_H_ */
