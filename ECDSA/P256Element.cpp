/*
 * P256Element.cpp
 *
 */

#include "P256Element.h"

#include "Math/gfp.hpp"

EC_GROUP* P256Element::curve;

void P256Element::init()
{
    curve = EC_GROUP_new_by_curve_name(NID_secp256k1);
    assert(curve != 0);
    auto modulus = EC_GROUP_get0_order(curve);
    Scalar::init_field(BN_bn2dec(modulus), false);
}

P256Element::P256Element()
{
    point = EC_POINT_new(curve);
    assert(point != 0);
    assert(EC_POINT_set_to_infinity(curve, point) != 0);
}

P256Element::P256Element(const Scalar& other) :
        P256Element()
{
    BIGNUM* exp = BN_new();
    BN_dec2bn(&exp, bigint(other).get_str().c_str());
    assert(EC_POINTs_mul(curve, point, exp, 0, 0, 0, 0) != 0);
    BN_free(exp);
}

P256Element::P256Element(word other) :
        P256Element()
{
    BIGNUM* exp = BN_new();
    BN_dec2bn(&exp, to_string(other).c_str());
    assert(EC_POINTs_mul(curve, point, exp, 0, 0, 0, 0) != 0);
    BN_free(exp);
}

P256Element& P256Element::operator =(const P256Element& other)
{
    assert(EC_POINT_copy(point, other.point) != 0);
    return *this;
}

void P256Element::check()
{
    assert(EC_POINT_is_on_curve(curve, point, 0) == 1);
}

P256Element::Scalar P256Element::x() const
{
    BIGNUM* x = BN_new();
    assert(EC_POINT_get_affine_coordinates_GFp(curve, point, x, 0, 0) != 0);
    char* xx = BN_bn2dec(x);
    Scalar res((bigint(xx)));
    OPENSSL_free(xx);
    BN_free(x);
    return res;
}

P256Element P256Element::operator +(const P256Element& other) const
{
    P256Element res;
    assert(EC_POINT_add(curve, res.point, point, other.point, 0) != 0);
    return res;
}

P256Element P256Element::operator -(const P256Element& other) const
{
    P256Element tmp = other;
    assert(EC_POINT_invert(curve, tmp.point, 0) != 0);
    return *this + tmp;
}

// P256Element P256Element::operator +(const P256Element::Scalar& other) const
// {
//     P256Element other_p(other);
//     P256Element res;
//     assert(EC_POINT_add(curve, res.point, point, other_p.point, 0) != 0);
//     return res;
// }

P256Element P256Element::operator *(const Scalar& other) const
{
    P256Element res;
    BIGNUM* exp = BN_new();
    BN_dec2bn(&exp, bigint(other).get_str().c_str());
    assert(EC_POINT_mul(curve, res.point, 0, point, exp, 0) != 0);
    BN_free(exp);
    return res;
}

bool P256Element::operator ==(const P256Element& other) const
{
    int cmp = EC_POINT_cmp(curve, point, other.point, 0);
    assert(cmp == 0 or cmp == 1);
    return not cmp;
}

void P256Element::pack(octetStream& os) const
{
    octet* buffer;
    size_t length = EC_POINT_point2buf(curve, point,
            POINT_CONVERSION_COMPRESSED, &buffer, 0);
    assert(length != 0);
    os.store_int(length, 8);
    os.append(buffer, length);
}

void P256Element::unpack(octetStream& os)
{
    size_t length = os.get_int(8);
    assert(
            EC_POINT_oct2point(curve, point, os.consume(length), length, 0)
                    != 0);
}

ostream& operator <<(ostream& s, const P256Element& x)
{
    char* hex = EC_POINT_point2hex(x.curve, x.point,
            POINT_CONVERSION_COMPRESSED, 0);
    s << hex;
    OPENSSL_free(hex);
    return s;
}

P256Element::P256Element(const P256Element& other) :
        P256Element()
{
    *this = other;
}

P256Element operator*(const P256Element::Scalar& x, const P256Element& y)
{
    return y * x;
}

P256Element& P256Element::operator +=(const P256Element& other)
{
    *this = *this + other;
    return *this;
}

P256Element& P256Element::operator /=(const Scalar& other)
{
    *this = *this * other.invert();
    return *this;
}

bool P256Element::operator !=(const P256Element& other) const
{
    return not (*this == other);
}

octetStream P256Element::hash(size_t n_bytes) const
{
    octetStream os;
    pack(os);
    auto res = os.hash();
    assert(n_bytes >= res.get_length());
    res.resize_precise(n_bytes);
    return res;
}

void P256Element::randomize(PRNG& G, int n)
{
    (void) n;
    P256Element::Scalar newscalar;
    newscalar.randomize(G, n);
    point = P256Element(newscalar).point;
}

void P256Element::input(istream& s,bool human)
{ 
    P256Element::Scalar newscalar;
    newscalar.input(s,human); 
    point = P256Element(newscalar).point;
}
