#pragma once

#include <QByteArray>
#include <QList>

/// Cuts a PGN file into its games as it is read, a piece at a time, so a
/// file of gigabytes is never held whole. The cut is PgnFile::scan's: a game
/// ends where the tags of the next one begin after its moves.
class PgnSplitter {
public:
    /// The games completed by `bytes`, in order.
    QList<QByteArray> feed(const QByteArray &bytes);
    /// The games left once the file is over: the last one.
    QList<QByteArray> finish();

private:
    /// The line [from, to) of m_pending joins the game, or begins the next.
    void takeLine(qsizetype from, qsizetype to, QList<QByteArray> &games);

    QByteArray m_pending; // The part of a line not ended yet.
    QByteArray m_game;    // The lines of the game being read.
    bool m_inMoves = false;
    bool m_hasTags = false;
};
