#include "Chapters.h"

#include <QCoreApplication>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(Chapters)
};

} // namespace

bool ChapterGame::isEmpty() const
{
    return game.moves.isEmpty() && game.startFen.isEmpty() && game.uid.isEmpty() && paragraphs.isEmpty();
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

int ChapterBook::addChapter(const QString &title)
{
    Chapter chapter;
    chapter.title = title.trimmed().isEmpty() ? defaultTitle(int(chapters.size()) + 1) : title.trimmed();
    chapters << chapter;
    current = int(chapters.size()) - 1;
    return current;
}

bool ChapterBook::removeChapter(int index)
{
    if (chapters.size() <= 1 || index < 0 || index >= chapters.size())
        return false;
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
    return index;
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

int ChapterBook::insertParagraph(int game, int ply, int after)
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
    paragraphs.insert(index, Paragraph{ply, QString()});
    return index;
}

void ChapterBook::setParagraph(int game, int index, const QString &text)
{
    QList<Paragraph> &paragraphs = chapter().games[game].paragraphs;
    if (index < 0 || index >= paragraphs.size())
        return;
    if (text.trimmed().isEmpty())
        paragraphs.removeAt(index);
    else
        paragraphs[index].text = text;
}
