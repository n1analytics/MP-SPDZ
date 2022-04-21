/*
 * Element.h
 *
 */

#ifndef ECDSA_P256ELEMENT_H_
#define ECDSA_P256ELEMENT_H_

#include <openssl/ec.h>
#include <openssl/obj_mac.h>

#include "Math/gfp.h"

class P256Element : public ValueInterface
{
public:
    typedef gfp_<2, 4> Scalar;
    typedef Scalar::next next; // no idea about sideeffects
    static const int MAX_N_BITS = 64 * 4;

private:
    static EC_GROUP* curve;

    EC_POINT* point;

public:
    // typedef void next;
    typedef void Square;


    static const true_type invertible;

    static int size() { return 0; }
    static string type_string() { return "P256"; }

    static void init();

    P256Element();
    P256Element(const P256Element& other);
    P256Element(const Scalar& other);
    P256Element(word other);

    P256Element& operator=(const P256Element& other);

    void randomize(PRNG& G, int n = -1);
    void input(istream& s, bool human);
    static string type_short() { return "ec"; }
    static DataFieldType field_type() { return DATA_INT; }
    
    void check();

    Scalar x() const;

    P256Element operator+(const P256Element& other) const;
    // P256Element operator+(const P256Element::Scalar& other) const;
    P256Element operator-(const P256Element& other) const;
    P256Element operator*(const Scalar& other) const;

    P256Element& operator+=(const P256Element& other);
    P256Element& operator/=(const Scalar& other);

    bool operator==(const P256Element& other) const;
    bool operator!=(const P256Element& other) const;

    void assign_zero() { *this = {}; }
    bool is_zero() { return *this == P256Element(); }
    void add(octetStream& os) { *this += os.get<P256Element>(); }

    void pack(octetStream& os) const;
    void unpack(octetStream& os);

    octetStream hash(size_t n_bytes) const;

    friend ostream& operator<<(ostream& s, const P256Element& x);
};

P256Element operator*(const P256Element::Scalar& x, const P256Element& y);

#endif /* ECDSA_P256ELEMENT_H_ */
