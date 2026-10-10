#include "PgnSplitter.h"

namespace {

/// The first character of a line that is not a space, or 0.
char firstChar(const char *line, qsizetype size)
{
    for (qsizetype i = 0; i < size; ++i) {
        const char c = line[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            return c;
    }
    return 0;
}

} // namespace

QList<QByteArray> PgnSplitter::feed(const QByteArray &bytes)
{
    QList<QByteArray> games;
    m_pending += bytes;
    qsizetype position = 0;
    for (;;) {
        const qsizetype newline = m_pending.indexOf('\n', position);
        if (newline < 0)
            break;
        takeLine(position, newline + 1, games);
        position = newline + 1;
    }
    m_pending.remove(0, position);
    return games;
}

QList<QByteArray> PgnSplitter::finish()
{
    QList<QByteArray> games;
    // A last line without its newline.
    if (!m_pending.isEmpty())
        takeLine(0, m_pending.size(), games);
    if (!m_game.trimmed().isEmpty())
        games << m_game;
    m_game.clear();
    m_pending.clear();
    m_inMoves = m_hasTags = false;
    return games;
}

void PgnSplitter::takeLine(qsizetype from, qsizetype to, QList<QByteArray> &games)
{
    const char first = firstChar(m_pending.constData() + from, to - from);
    if (first == '[') {
        // The tags of the next game: the one before ends here.
        if (m_inMoves && m_hasTags) {
            games << m_game;
            m_game.clear();
        }
        m_inMoves = false;
        m_hasTags = true;
    } else if (first != 0 && first != '%') {
        m_inMoves = true;
    }
    m_game.append(m_pending.constData() + from, to - from);
}
