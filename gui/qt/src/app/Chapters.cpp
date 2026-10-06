#include "Chapters.h"

#include <QCoreApplication>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(Chapters)
};

} // namespace

QString Paragraph::kindKey(Kind kind)
{
    switch (kind) {
    case Kind::Title:
        return QStringLiteral("title");
    case Kind::Subtitle:
        return QStringLiteral("subtitle");
    case Kind::Text:
        break;
    }
    return QStringLiteral("text");
}

Paragraph::Kind Paragraph::kindFromKey(const QString &key)
{
    if (key == QLatin1String("title"))
        return Kind::Title;
    if (key == QLatin1String("subtitle"))
        return Kind::Subtitle;
    return Kind::Text;
}

bool ChapterGame::isEmpty() const
{
    return game.moves.isEmpty() && game.startFen.isEmpty() && game.uid.isEmpty() && paragraphs.isEmpty();
}

bool ChapterGame::holdsWork() const
{
    return !paragraphs.isEmpty() || (game.uid.isEmpty() && (!game.moves.isEmpty() || !game.startFen.isEmpty()));
}

ChapterBook::ChapterBook()
{
    Chapter first;
    first.title = defaultTitle(1);
    chapters << first;
}

QString ChapterBook::defaultTitle(int number)
{
    return Text::tr("Chapter %1").arg(number);
}

bool ChapterBook::hasChapters() const
{
    return !m_none;
}

void ChapterBook::clear()
{
    *this = ChapterBook();
}

void ChapterBook::setChapters(const QList<Chapter> &list, int open, bool none, bool automatic)
{
    if (list.isEmpty()) {
        clear();
        return;
    }
    chapters = list;
    current = qBound(0, open, int(list.size()) - 1);
    m_none = none && list.size() == 1;
    m_automatic = m_none || automatic;
    settle();
}

void ChapterBook::settle()
{
    if (!m_none)
        return;
    // The game on the board alone is not a chapter, whatever its moves: a
    // second game, a paragraph, a title are.
    const Chapter &held = chapters.first();
    if (chapters.size() > 1 || held.games.size() > 1 || !held.games.first().paragraphs.isEmpty())
        m_none = false;
}

void ChapterBook::settleAfterRemoval()
{
    if (m_none || !m_automatic || chapters.size() != 1)
        return;
    const Chapter &only = chapters.first();
    if (only.games.size() != 1)
        return;
    if (only.games.first().paragraphs.isEmpty()) {
        m_none = true;
        current = 0;
    }
}

int ChapterBook::addChapter(const QString &title)
{
    settle();
    m_automatic = false; // A chapter asked for: it stays.
    if (m_none) {
        // What is there, nothing yet, becomes the first chapter.
        m_none = false;
        chapters.first().title = title.trimmed().isEmpty() ? defaultTitle(1) : title.trimmed();
        current = 0;
        return current;
    }
    Chapter chapter;
    chapter.title = title.trimmed().isEmpty() ? defaultTitle(int(chapters.size()) + 1) : title.trimmed();
    chapters << chapter;
    current = int(chapters.size()) - 1;
    return current;
}

bool ChapterBook::removeChapter(int index)
{
    if (index < 0 || index >= chapters.size() || m_none)
        return false;
    if (chapters.size() == 1) {
        clear();
        return true;
    }
    chapters.removeAt(index);
    if (current > index || current >= chapters.size())
        current = qMax(0, current - 1);
    return true;
}

void ChapterBook::moveChapter(int from, int to)
{
    if (from < 0 || from >= chapters.size() || to < 0 || to >= chapters.size() || from == to)
        return;
    const int open = current;
    chapters.move(from, to);
    // The open chapter stays open, wherever it went.
    if (open == from)
        current = to;
    else if (from < open && to >= open)
        current = open - 1;
    else if (from > open && to <= open)
        current = open + 1;
}

int ChapterBook::insertGame(int after)
{
    Chapter &open = chapter();
    const int index = qBound(0, after + 1, int(open.games.size()));
    open.games.insert(index, ChapterGame());
    open.currentGame = index;
    open.ply = 0;
    m_none = false; // A game break is something put in the chapter.
    return index;
}

int ChapterBook::breakGame()
{
    Chapter &open = chapter();
    if (open.games.last().isEmpty()) {
        open.currentGame = int(open.games.size()) - 1;
        open.ply = 0;
    } else {
        insertGame(int(open.games.size()) - 1);
    }
    m_none = false;
    return open.currentGame;
}

bool ChapterBook::removeGame(int index)
{
    Chapter &open = chapter();
    if (index < 0 || index >= open.games.size())
        return false;
    if (open.games.size() == 1) {
        // A chapter always has a game: the only one is emptied.
        if (open.games.first().isEmpty())
            return false;
        open.games.first() = ChapterGame();
        open.ply = 0;
    } else {
        open.games.removeAt(index);
        if (open.currentGame == index) {
            // The game before; for the first, the one that came after it.
            open.currentGame = qMax(0, index - 1);
            open.ply = index > 0 ? int(open.games.at(open.currentGame).game.moves.size()) : 0;
        } else if (open.currentGame > index) {
            --open.currentGame;
        }
    }
    settleAfterRemoval();
    return true;
}

void ChapterBook::moveGame(int from, int to)
{
    Chapter &open = chapter();
    if (from < 0 || from >= open.games.size() || to < 0 || to >= open.games.size() || from == to)
        return;
    const int current = open.currentGame;
    open.games.move(from, to);
    if (current == from)
        open.currentGame = to;
    else if (from < current && to >= current)
        open.currentGame = current - 1;
    else if (from > current && to <= current)
        open.currentGame = current + 1;
}

int ChapterBook::findGame(const QString &uid) const
{
    if (uid.isEmpty())
        return -1;
    const QList<ChapterGame> &games = chapter().games;
    for (int i = 0; i < games.size(); ++i) {
        if (games.at(i).game.uid == uid)
            return i;
    }
    return -1;
}

int ChapterBook::insertParagraph(int game, int ply, int after, Paragraph::Kind kind)
{
    QList<Paragraph> &paragraphs = chapter().games[game].paragraphs;
    int index = 0;
    if (after >= 0 && after < paragraphs.size()) {
        index = after + 1;
        ply = paragraphs.at(after).ply;
    } else {
        // Below the ones already after that move.
        while (index < paragraphs.size() && paragraphs.at(index).ply <= ply)
            ++index;
    }
    paragraphs.insert(index, Paragraph{ply, QString(), kind});
    m_none = false;
    return index;
}

int ChapterBook::moveParagraph(int game, int index, int ply, bool first)
{
    QList<Paragraph> &paragraphs = chapter().games[game].paragraphs;
    if (index < 0 || index >= paragraphs.size())
        return index;
    Paragraph moved = paragraphs.takeAt(index);
    moved.ply = qMax(0, ply);
    int to = 0;
    while (to < paragraphs.size() && (paragraphs.at(to).ply < moved.ply || (!first && paragraphs.at(to).ply == moved.ply)))
        ++to;
    paragraphs.insert(to, moved);
    return to;
}

void ChapterBook::setParagraph(int game, int index, const QString &text)
{
    QList<Paragraph> &paragraphs = chapter().games[game].paragraphs;
    if (index < 0 || index >= paragraphs.size())
        return;
    if (text.trimmed().isEmpty())
        removeParagraph(game, index);
    else
        paragraphs[index].text = text;
}

void ChapterBook::removeParagraph(int game, int index)
{
    QList<Paragraph> &paragraphs = chapter().games[game].paragraphs;
    if (index < 0 || index >= paragraphs.size())
        return;
    paragraphs.removeAt(index);
    settleAfterRemoval();
}
