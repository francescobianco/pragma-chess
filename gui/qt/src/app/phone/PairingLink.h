#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

/// The link shown as a QR code by File ▸ Phone Access…: who the computer is
/// (its Nostr public key and name), where to find it (relays) and the one-time
/// secret that lets a new phone pair (see docs/phone-link.md).
///
///     pragma-chess://pair?k=<pubkey hex>&s=<secret hex>&n=<name>&r=<relay>&r=…
struct PairingLink {
    QByteArray computerKey; // 32 bytes
    QByteArray secret;      // 32 bytes
    QString computerName;
    QStringList relays;

    QString toString() const;
    /// The link of a string, if it is a well-formed pairing link.
    static std::optional<PairingLink> parse(const QString &text);
};
