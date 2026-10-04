#pragma once

#include "app/GameRecord.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

/// A PGN file as a list of games, read and written byte for byte: what the
/// PGN file source (PgnFileFetch) works on. Pure, unit-tested.
///
/// The file is cut into entries that cover it whole, in order: each from the
/// first tag of a game to the first tag of the next (what precedes the first
/// game belongs to the first entry). Putting the entries back together gives
/// the file again, so a game that is not touched keeps its bytes — comments,
/// spacing, line endings.
namespace PgnFile {

/// The tag that ties a game of the file to a game of the database (its uid).
inline constexpr char uidTag[] = "PragmaUid";

struct Entry {
    qint64 offset = 0;
    qint64 length = 0;
    /// Short hash of the entry's text, line endings and outer spaces aside:
    /// whether the game changed.
    QString hash;
    /// The PragmaUid tag, empty when the game has none.
    QString uid;
    /// Whether there is a game in it (tags or moves), not only spaces.
    bool isGame = false;
};

/// Cuts `bytes` into entries.
QList<Entry> scan(const QByteArray &bytes);

/// The text of a file: UTF-8, or Latin-1 (PGN's own) when it is not valid UTF-8.
QString decode(const QByteArray &bytes);

/// The game of an entry, with its uid tag as uid; nothing, and why, when its
/// moves cannot be read.
std::optional<GameRecord> read(const QByteArray &entry, QString *errorMessage);

/// A game as an entry: its PGN with the uid tag, and a blank line after it.
QByteArray write(const GameRecord &game);

/// `entry` with the uid tag after its other tags (replacing one it has),
/// everything else as it was.
QByteArray withUid(const QByteArray &entry, const QString &uid);

/// The SHA-256 of a file's bytes, in hex.
QString fileHash(const QByteArray &bytes);

/// The index of a file: its hash and its entries, kept beside it so a file
/// that did not change is not read game by game again.
struct Index {
    QString fileHash;
    QList<Entry> entries;
};

/// Where the index of `pgnPath` lives: a hidden file beside it,
/// ".<name>.pragma-index".
QString indexPath(const QString &pgnPath);
/// The index of `pgnPath` when it matches `hash`, the file as it is now.
std::optional<Index> readIndex(const QString &pgnPath, const QString &hash);
/// Best effort: a folder that cannot be written only means scanning again.
void writeIndex(const QString &pgnPath, const Index &index);

} // namespace PgnFile
