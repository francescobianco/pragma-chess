#include "Nip44.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

#include <algorithm>
#include <iterator>

namespace Nip44 {

namespace {

constexpr int kMinPlaintext = 1;
constexpr int kMaxPlaintext = 65535;

QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
}

quint32 rotl(quint32 value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

void quarterRound(quint32 &a, quint32 &b, quint32 &c, quint32 &d)
{
    a += b; d ^= a; d = rotl(d, 16);
    c += d; b ^= c; b = rotl(b, 12);
    a += b; d ^= a; d = rotl(d, 8);
    c += d; b ^= c; b = rotl(b, 7);
}

quint32 readLittleEndian(const char *bytes)
{
    const auto *u = reinterpret_cast<const unsigned char *>(bytes);
    return quint32(u[0]) | quint32(u[1]) << 8 | quint32(u[2]) << 16 | quint32(u[3]) << 24;
}

bool equalConstantTime(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return difference == 0;
}

std::optional<QByteArray> pad(const QByteArray &plaintext)
{
    const int length = int(plaintext.size());
    if (length < kMinPlaintext || length > kMaxPlaintext)
        return std::nullopt;
    QByteArray padded;
    padded.reserve(2 + paddedLength(length));
    padded.append(char(length >> 8));
    padded.append(char(length & 0xff));
    padded.append(plaintext);
    padded.append(QByteArray(paddedLength(length) - length, '\0'));
    return padded;
}

std::optional<QByteArray> unpad(const QByteArray &padded)
{
    if (padded.size() < 2)
        return std::nullopt;
    const int length = int(static_cast<unsigned char>(padded.at(0))) << 8 | static_cast<unsigned char>(padded.at(1));
    if (length < kMinPlaintext || 2 + length > padded.size() || padded.size() != 2 + paddedLength(length))
        return std::nullopt;
    return padded.mid(2, length);
}

} // namespace

QByteArray chacha20(const QByteArray &key, const QByteArray &nonce, const QByteArray &data)
{
    quint32 state[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
    for (int i = 0; i < 8; ++i)
        state[4 + i] = readLittleEndian(key.constData() + 4 * i);
    state[12] = 0;
    for (int i = 0; i < 3; ++i)
        state[13 + i] = readLittleEndian(nonce.constData() + 4 * i);

    QByteArray output = data;
    for (qsizetype offset = 0; offset < output.size(); offset += 64) {
        quint32 block[16];
        std::copy(std::begin(state), std::end(state), block);
        for (int round = 0; round < 10; ++round) {
            quarterRound(block[0], block[4], block[8], block[12]);
            quarterRound(block[1], block[5], block[9], block[13]);
            quarterRound(block[2], block[6], block[10], block[14]);
            quarterRound(block[3], block[7], block[11], block[15]);
            quarterRound(block[0], block[5], block[10], block[15]);
            quarterRound(block[1], block[6], block[11], block[12]);
            quarterRound(block[2], block[7], block[8], block[13]);
            quarterRound(block[3], block[4], block[9], block[14]);
        }
        for (int i = 0; i < 16; ++i)
            block[i] += state[i];
        for (int i = 0; i < 64 && offset + i < output.size(); ++i)
            output[offset + i] = char(output.at(offset + i) ^ char(block[i / 4] >> (8 * (i % 4))));
        ++state[12];
    }
    return output;
}

QByteArray conversationKey(const QByteArray &sharedX)
{
    return hmacSha256(QByteArrayLiteral("nip44-v2"), sharedX);
}

MessageKeys messageKeys(const QByteArray &conversationKey, const QByteArray &nonce)
{
    // HKDF-expand to 76 bytes: three SHA-256 blocks.
    QByteArray output;
    QByteArray previous;
    for (char counter = 1; output.size() < 76; ++counter) {
        previous = hmacSha256(conversationKey, previous + nonce + QByteArray(1, counter));
        output += previous;
    }
    return {output.left(32), output.mid(32, 12), output.mid(44, 32)};
}

int paddedLength(int length)
{
    if (length <= 32)
        return 32;
    int nextPower = 1;
    while (nextPower < length)
        nextPower <<= 1;
    const int chunk = nextPower <= 256 ? 32 : nextPower / 8;
    return chunk * ((length - 1) / chunk + 1);
}

std::optional<QString> encrypt(const QByteArray &plaintext, const QByteArray &conversationKey)
{
    QByteArray nonce(32, Qt::Uninitialized);
    QRandomGenerator::system()->generate(nonce.begin(), nonce.end());
    return encrypt(plaintext, conversationKey, nonce);
}

std::optional<QString> encrypt(const QByteArray &plaintext, const QByteArray &conversationKey,
                               const QByteArray &nonce)
{
    if (conversationKey.size() != 32 || nonce.size() != 32)
        return std::nullopt;
    const std::optional<QByteArray> padded = pad(plaintext);
    if (!padded)
        return std::nullopt;
    const MessageKeys keys = messageKeys(conversationKey, nonce);
    const QByteArray ciphertext = chacha20(keys.chachaKey, keys.chachaNonce, *padded);
    const QByteArray mac = hmacSha256(keys.hmacKey, nonce + ciphertext);
    return QString::fromLatin1((QByteArray(1, '\x02') + nonce + ciphertext + mac).toBase64());
}

std::optional<QByteArray> decrypt(const QString &payload, const QByteArray &conversationKey)
{
    if (conversationKey.size() != 32 || payload.isEmpty() || payload.startsWith(QLatin1Char('#')))
        return std::nullopt;
    if (payload.size() < 132 || payload.size() > 87472)
        return std::nullopt;
    const auto decoded = QByteArray::fromBase64Encoding(payload.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
        return std::nullopt;
    const QByteArray data = *decoded;
    if (data.size() < 99 || data.size() > 65603 || data.at(0) != 2)
        return std::nullopt;
    const QByteArray nonce = data.mid(1, 32);
    const QByteArray ciphertext = data.mid(33, data.size() - 33 - 32);
    const QByteArray mac = data.right(32);
    const MessageKeys keys = messageKeys(conversationKey, nonce);
    if (!equalConstantTime(hmacSha256(keys.hmacKey, nonce + ciphertext), mac))
        return std::nullopt;
    return unpad(chacha20(keys.chachaKey, keys.chachaNonce, ciphertext));
}

} // namespace Nip44
