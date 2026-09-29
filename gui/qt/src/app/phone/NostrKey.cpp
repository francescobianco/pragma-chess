#include "NostrKey.h"

#include <QRandomGenerator>

#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>

#include <cstring>

namespace {

QByteArray randomBytes(int size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    QRandomGenerator::system()->generate(bytes.begin(), bytes.end());
    return bytes;
}

const unsigned char *bytesOf(const QByteArray &data)
{
    return reinterpret_cast<const unsigned char *>(data.constData());
}

// One context for the process: secp256k1 contexts are safe to share for
// signing and verifying once randomized.
const secp256k1_context *context()
{
    static secp256k1_context *const ctx = [] {
        secp256k1_context *created = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
        const QByteArray seed = randomBytes(32);
        [[maybe_unused]] const int randomized = secp256k1_context_randomize(created, bytesOf(seed));
        return created;
    }();
    return ctx;
}

int copyX(unsigned char *output, const unsigned char *x32, const unsigned char *, void *)
{
    std::memcpy(output, x32, 32);
    return 1;
}

} // namespace

NostrKey::NostrKey(QByteArray secret, QByteArray publicKey)
    : m_secret(std::move(secret))
    , m_publicKey(std::move(publicKey))
{
}

NostrKey NostrKey::generate()
{
    for (;;) {
        if (std::optional<NostrKey> key = fromSecret(randomBytes(32)))
            return *key;
    }
}

std::optional<NostrKey> NostrKey::fromSecret(const QByteArray &secret)
{
    if (secret.size() != 32)
        return std::nullopt;
    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(context(), &keypair, bytesOf(secret)))
        return std::nullopt;
    secp256k1_xonly_pubkey xonly;
    secp256k1_keypair_xonly_pub(context(), &xonly, nullptr, &keypair);
    QByteArray publicKey(32, Qt::Uninitialized);
    secp256k1_xonly_pubkey_serialize(context(), reinterpret_cast<unsigned char *>(publicKey.data()), &xonly);
    return NostrKey(secret, publicKey);
}

QByteArray NostrKey::sign(const QByteArray &hash) const
{
    if (hash.size() != 32)
        return {};
    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(context(), &keypair, bytesOf(m_secret)))
        return {};
    const QByteArray aux = randomBytes(32);
    QByteArray signature(64, Qt::Uninitialized);
    if (!secp256k1_schnorrsig_sign32(context(), reinterpret_cast<unsigned char *>(signature.data()), bytesOf(hash),
                                     &keypair, bytesOf(aux)))
        return {};
    return signature;
}

bool NostrKey::verify(const QByteArray &publicKey, const QByteArray &message, const QByteArray &signature)
{
    if (publicKey.size() != 32 || signature.size() != 64)
        return false;
    secp256k1_xonly_pubkey xonly;
    if (!secp256k1_xonly_pubkey_parse(context(), &xonly, bytesOf(publicKey)))
        return false;
    return secp256k1_schnorrsig_verify(context(), bytesOf(signature), bytesOf(message), size_t(message.size()),
                                       &xonly) == 1;
}

std::optional<QByteArray> NostrKey::sharedX(const QByteArray &peerPublicKey) const
{
    if (peerPublicKey.size() != 32)
        return std::nullopt;
    const QByteArray compressed = QByteArray(1, '\x02') + peerPublicKey;
    secp256k1_pubkey point;
    if (!secp256k1_ec_pubkey_parse(context(), &point, bytesOf(compressed), size_t(compressed.size())))
        return std::nullopt;
    QByteArray shared(32, Qt::Uninitialized);
    if (!secp256k1_ecdh(context(), reinterpret_cast<unsigned char *>(shared.data()), &point, bytesOf(m_secret), copyX,
                        nullptr))
        return std::nullopt;
    return shared;
}
