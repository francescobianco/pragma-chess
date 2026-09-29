#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

/// A Nostr key pair: a secp256k1 secret key and its x-only public key
/// (BIP-340). Keys and signatures are raw bytes; Nostr writes them in hex.
class NostrKey {
public:
    /// A new random key.
    static NostrKey generate();
    /// The key for a 32-byte secret, if it is a valid secp256k1 secret key.
    static std::optional<NostrKey> fromSecret(const QByteArray &secret);

    QByteArray secret() const { return m_secret; }
    /// The 32-byte x-only public key.
    QByteArray publicKey() const { return m_publicKey; }
    QString publicKeyHex() const { return QString::fromLatin1(m_publicKey.toHex()); }

    /// BIP-340 Schnorr signature (64 bytes) of a 32-byte hash, with fresh
    /// auxiliary randomness.
    QByteArray sign(const QByteArray &hash) const;
    /// Whether `signature` is a valid BIP-340 signature of `message` by the
    /// x-only `publicKey`.
    static bool verify(const QByteArray &publicKey, const QByteArray &message, const QByteArray &signature);

    /// The x coordinate of the ECDH shared point with an x-only public key
    /// (taken with even y, as NIP-44 does), or nothing if the key is invalid.
    std::optional<QByteArray> sharedX(const QByteArray &peerPublicKey) const;

private:
    NostrKey(QByteArray secret, QByteArray publicKey);

    QByteArray m_secret;
    QByteArray m_publicKey;
};
