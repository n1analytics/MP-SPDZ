/*
 * sign.hpp
 *
 */

#ifndef ECDSA_SIGN_HPP_
#define ECDSA_SIGN_HPP_

//#include "CurveElement.h"
#include "P256Element.h"
#include "Tools/Bundle.h"

#include "preprocessing.hpp"
#include "Math/gfp.hpp"

class EcSignature
{
public:
    P256Element R;
    P256Element::Scalar s;
};

/*
inline
CurveElement::Scalar hash_to_scalar(const unsigned char* message, size_t length, CurveElement::Scalar rx, CurveElement pk)
{
    crypto_hash_sha512_state state;
    crypto_hash_sha512_init(&state);
    crypto_hash_sha512_update(&state, (unsigned char*) rx.get_ptr(),
            crypto_core_ristretto255_SCALARBYTES);
    crypto_hash_sha512_update(&state, pk.get(), crypto_core_ristretto255_BYTES);
    crypto_hash_sha512_update(&state, message, length);
    unsigned char hash[crypto_hash_sha512_BYTES];
    crypto_hash_sha512_final(&state, hash);
    auto& tmp = bigint::tmp;
    mpz_import(tmp.get_mpz_t(), crypto_hash_sha512_BYTES, -1, 1, 0, 0, hash);
    return tmp;
}
*/

inline P256Element::Scalar hash_to_scalar(const unsigned char* message, size_t length)
{
    P256Element::Scalar res;
    assert(res.size() == crypto_hash_sha256_BYTES);
    crypto_hash_sha256((unsigned char*) res.get_ptr(), message, length);
    res.zero_overhang();
    return res;
}

template<template<class U> class T>
EcSignature sign(const unsigned char* message, size_t length,
        EcTuple<T> tuple,
        typename T<P256Element::Scalar>::MAC_Check& MC,
        typename T<P256Element>::MAC_Check& MCc,
        Player& P,
        EcdsaOptions opts,
        P256Element pk,
        T<P256Element::Scalar> sk = {},
        SubProcessor<T<P256Element::Scalar>>* proc = 0)
{
    (void) pk;
    Timer timer;
    timer.start();
    auto stats = P.total_comm();
    EcSignature signature;
    vector<P256Element> opened_R;
    if (opts.R_after_msg)
        MCc.POpen_Begin(opened_R, {tuple.secret_R}, P);
    T<P256Element::Scalar> prod = tuple.b;
    auto& protocol = proc->protocol;
    if (proc)
    {
        protocol.init_mul();
        protocol.prepare_mul(sk, tuple.a);
        protocol.start_exchange();
    }
    if (opts.R_after_msg)
    {
        MCc.POpen_End(opened_R, {tuple.secret_R}, P);
        tuple.R = opened_R[0];
        if (opts.fewer_rounds)
            tuple.R /= tuple.c;
    }
    if (proc)
    {
        protocol.stop_exchange();
        prod = protocol.finalize_mul();
    }
    signature.R = tuple.R;
    auto rx = tuple.R.x();
    signature.s = MC.open(
            tuple.a * hash_to_scalar(message, length) + prod * rx, P);
    auto diff = (P.total_comm() - stats);
    cout << "Minimal signing took " << timer.elapsed() * 1e3 << " ms and sending "
            << diff.sent << " bytes" << endl;
    diff.print(true);
    return signature;
}

inline
EcSignature sign(const unsigned char* message, size_t length, P256Element::Scalar sk)
{
    EcSignature signature;
    auto k = SeededPRNG().get<P256Element::Scalar>();
    signature.R = k;
    auto rx = signature.R.x();
    signature.s = k.invert() * (hash_to_scalar(message, length) + rx * sk);
    return signature;
}

inline
void check(EcSignature signature, const unsigned char* message, size_t length,
        P256Element pk)
{
    Timer timer;
    timer.start();
    signature.s.check();
    signature.R.check();
    auto w = signature.s.invert();
    auto u1 = hash_to_scalar(message, length) * w;
    auto u2 = signature.R.x() * w;
    assert(P256Element(u1) + pk * u2 == signature.R);
    cout << "Offline checking took " << timer.elapsed() * 1e3 << " ms" << endl;
}

template<template<class U> class T>
void sign_benchmark(vector<EcTuple<T>>& tuples, T<P256Element::Scalar> sk,
        typename T<P256Element::Scalar>::MAC_Check& MCp, Player& P,
        EcdsaOptions& opts,
        SubProcessor<T<P256Element::Scalar>>* proc = 0)
{
    unsigned char message[1024];

    // Random message
    GlobalPRNG(P).get_octets(message, 1024);
    
    typename T<P256Element>::Direct_MC MCc(MCp.get_alphai());

    // synchronize
    Bundle<octetStream> bundle(P);
    P.unchecked_broadcast(bundle);

    Timer timer;
    timer.start();
    auto stats = P.total_comm();

    // sk*P is private key, hence sk is the public key
    P256Element pk = MCc.open(sk, P);
    MCc.Check(P);
    cout << "Public key generation took " << timer.elapsed() * 1e3 << " ms" << endl;
    (P.total_comm() - stats).print(true);

    for (size_t i = 0; i < min(10lu, tuples.size()); i++)
    {
        check(sign(message, 1 << i, tuples[i], MCp, MCc, P, opts, pk, sk, proc), message,
                1 << i, pk);
        if (not opts.check_open)
            continue;
        Timer timer;
        timer.start();
        auto& check_player = MCp.get_check_player(P);
        auto stats = check_player.total_comm();
        MCp.Check(P);
        MCc.Check(P);
        auto diff = (check_player.total_comm() - stats);
        cout << "Online checking took " << timer.elapsed() * 1e3 << " ms and sending "
            << diff.sent << " bytes" << endl;
        diff.print();
    }
}

#endif /* ECDSA_SIGN_HPP_ */
