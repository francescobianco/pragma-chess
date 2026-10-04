#pragma once

#include "GameRecord.h"

#include <QList>
#include <QString>

/// Text between the moves of a chapter: after the main-line move `ply` of
/// its game (0: before the first move).
struct Paragraph {
    int ply = 0;
    QString text;

    bool operator==(const Paragraph &) const = default;
};

/// A game of a chapter, with the paragraphs written around its moves. A game
/// with a uid is stored in a database (and written back there as it
/// changes); `game` is its content as last seen, so the chapter can still be
/// shown where the database is not.
struct ChapterGame {
    GameRecord game;
    /// In order: by ply, and in the order written at the same ply.
    QList<Paragraph> paragraphs;

    /// Nothing in it yet: no moves, no position of its own, no paragraph,
    /// not a stored game. Such a game is taken over by the next one opened.
    bool isEmpty() const;
};

/// A chapter: games one after the other, as a chess book has them, with
/// text between their moves. The project is a list of chapters — a study,
/// or a book.
struct Chapter {
    QString title;
    QList<ChapterGame> games{ChapterGame()};
    /// Where the user was in it: the game, and the ply on its main line.
    int currentGame = 0;
    int ply = 0;
};

/// The chapters of a project and the one open. Never empty: there is always
/// a chapter, and a chapter always has a game. Pure, unit-tested.
class ChapterBook {
public:
    ChapterBook();

    QList<Chapter> chapters;
    int current = 0;

    Chapter &chapter() { return chapters[current]; }
    const Chapter &chapter() const { return chapters.at(current); }
    ChapterGame &game() { return chapter().games[chapter().currentGame]; }
    const ChapterGame &game() const { return chapter().games.at(chapter().currentGame); }

    /// "Chapter 3": the name a new chapter gets.
    static QString defaultTitle(int number);

    /// A new chapter at the end, with an empty game; it becomes the one open.
    int addChapter(const QString &title);
    /// False for the last chapter left: a project always has one.
    bool removeChapter(int index);
    void moveChapter(int from, int to);

    /// A new game, from the starting position, after the game `after` of the
    /// open chapter; it becomes the current one. Returns its index.
    int insertGame(int after);
    /// Insert Game Break: a new game at the end of the open chapter — every
    /// game but the last has a break after it already —, or the empty one
    /// already there. It becomes the current one; returns its index.
    int breakGame();
    /// Takes away the empty games of the open chapter (breaks with nothing
    /// after them) but the current one, which keeps its place in the list.
    void removeEmptyGames();
    /// The game of the open chapter stored under `uid`, or -1.
    int findGame(const QString &uid) const;

    /// A new paragraph in the game `game` of the open chapter: after the
    /// main-line move `ply`, below the paragraphs already there (or right
    /// after the paragraph `after` when it is given). Returns its index.
    int insertParagraph(int game, int ply, int after = -1);
    /// Moves a paragraph of the game `game` of the open chapter after the
    /// main-line ply `ply` (0: before the first move), first or last among
    /// the paragraphs already there. Returns its new index.
    int moveParagraph(int game, int index, int ply, bool first);
    /// Sets the text of a paragraph; an empty text removes it.
    void setParagraph(int game, int index, const QString &text);
};
