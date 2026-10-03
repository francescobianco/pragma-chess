#pragma once

#include "SourceFetch.h"

/// The games of a ChessBase database on this computer (ChessBaseDatabase),
/// read from where the last sync stopped: a database that grows is picked
/// up game by game, and nothing is read twice. The file stays where it is.
///
/// Settings: {"path": "/…/Base.cbh"}. State: {"read": <records read so far>}.
class ChessBaseFetch : public SourceFetch {
    Q_OBJECT

public:
    explicit ChessBaseFetch(const GameSource &source, QObject *parent = nullptr);

    void start() override;
    void abort() override;

    /// The `.cbh` file of a source of this kind.
    static QString path(const GameSource &source);
    /// Whether that file is where the source says.
    static bool isAvailable(const GameSource &source);

private:
    void readBatch();

    GameSource m_source;
    bool m_aborted = false;
    int m_next = 0;
};

namespace ChessBaseSettings {
inline constexpr char path[] = "path";
inline constexpr char read[] = "read";
} // namespace ChessBaseSettings
