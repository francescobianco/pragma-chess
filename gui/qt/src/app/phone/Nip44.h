#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

/// NIP-44 version 2: the encryption of Nostr direct messages, used here to
/// hide the WebRTC offers and answers from the relays that carry them.
///
/// Pure functions on raw bytes; tested against the official test vectors.
namespace Nip44 {

/// The per-message keys derived from a conversation key and a nonce.
struct MessageKeys {
    QByteArray chachaKey;   // 32 bytes
    QByteArray chachaNonce; // 12 bytes
    QByteArray hmacKey;     // 32 bytes
};

/// HKDF-extract of the ECDH shared x coordinate (NostrKey::sharedX).
QByteArray conversationKey(const QByteArray &sharedX);
MessageKeys messageKeys(const QByteArray &conversationKey, const QByteArray &nonce);
/// Length of a padded plaintext of `length` bytes (1 ≤ length ≤ 65535).
int paddedLength(int length);

/// The base64 payload of `plaintext` (UTF-8, 1–65535 bytes) with a random nonce,
/// or nothing when the plaintext is empty or too long.
std::optional<QString> encrypt(const QByteArray &plaintext, const QByteArray &conversationKey);
/// Same with a given 32-byte nonce, for the test vectors.
std::optional<QString> encrypt(const QByteArray &plaintext, const QByteArray &conversationKey,
                               const QByteArray &nonce);
/// The plaintext of a payload, or nothing if it is malformed, of another
/// version or not authentic.
std::optional<QByteArray> decrypt(const QString &payload, const QByteArray &conversationKey);

/// RFC 8439 ChaCha20 keystream XOR, starting from block counter 0.
QByteArray chacha20(const QByteArray &key, const QByteArray &nonce, const QByteArray &data);

} // namespace Nip44
