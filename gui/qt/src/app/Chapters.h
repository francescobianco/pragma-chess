#pragma once

#include "GameRecord.h"

#include <QList>
#include <QString>

/// Text between the moves of a chapter: after the main-line move `ply` of
/// its game (0: before the first move).
struct Paragraph {
    /// How the text is set: a paragraph of a book, or a heading — a title,
    /// bold and centred, or a subtitle, bold, smaller and centred too.
    enum class Kind { Text, Title, Subtitle };

    int ply = 0;
    QString text;
    Kind kind = Kind::Text;

    /// "title", "subtitle", "text": the kind as the project file writes it.
    static QString kindKey(Kind kind);
    /// The kind of a key; anything unknown is Text.
    static Kind kindFromKey(const QString &key);

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

/// The chapters of a project and the one open. A new, empty project has no
/// chapter: `chapters` then holds one all the same, not shown as a chapter,
/// where whatever the user does lands (the game on the board, paragraphs,
/// game breaks); as soon as something is in it, it becomes the first
/// chapter. So `chapters` is never empty, and a chapter always has a game.
/// Pure, unit-tested.
class ChapterBook {
public:
    ChapterBook();

    QList<Chapter> chapters;
    int current = 0;

    /// Whether the project has chapters, or is still without (the menus say
    /// "(No Chapter)").
    bool hasChapters() const;
    /// Back to no chapter: everything in the chapters goes.
    void clear();
    /// The chapters of a project, and the one open; an empty list, or one
    /// held with `none`, is no chapter. `automatic`: they came by themselves
    /// (isAutomatic).
    void setChapters(const QList<Chapter> &list, int open, bool none = false, bool automatic = false);
    /// The chapters came by themselves — things were put in a project without
    /// chapters —, not by New Chapter or by renaming them: taking those things
    /// away again leaves the project without chapters (settleAfterRemoval).
    bool isAutomatic() const { return m_automatic; }
    /// Something was done: if the project had no chapter and the one held
    /// now has something in it, it becomes the first chapter: moves of a game
    /// not stored, a paragraph, a second game — not a game of the database
    /// only opened, which the next one opened replaces. Inserting a game or a
    /// paragraph does it by itself.
    void settle();
    /// Something was taken away: if the chapters came by themselves and what
    /// is left is what a project without chapters holds — one game, a game
    /// of the database only looked at or nothing at all, no paragraph —,
    /// the project is without chapters again. The removals here do it.
    void settleAfterRemoval();

    Chapter &chapter() { return chapters[current]; }
    const Chapter &chapter() const { return chapters.at(current); }
    ChapterGame &game() { return chapter().games[chapter().currentGame]; }
    const ChapterGame &game() const { return chapter().games.at(chapter().currentGame); }

    /// "Chapter 3": the name a new chapter gets.
    static QString defaultTitle(int number);

    /// A new chapter at the end, with an empty game; it becomes the one open.
    /// In a project without chapters, the first chapter is what is there.
    int addChapter(const QString &title);
    /// Removing the last chapter leaves the project without chapters.
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
    /// Takes the game `index` of the open chapter away, with the break before
    /// it: Delete Game Break for an empty one, Delete Following Game for one
    /// with something in it. The first game has no break and stays. The
    /// current game, if it goes, is the one before. Returns whether it went.
    bool removeGame(int index);
    /// Moves a game of the open chapter to the place `to`; the current game
    /// stays current wherever it goes.
    void moveGame(int from, int to);
    /// The game of the open chapter stored under `uid`, or -1.
    int findGame(const QString &uid) const;

    /// A new paragraph in the game `game` of the open chapter: after the
    /// main-line move `ply`, below the paragraphs already there (or right
    /// after the paragraph `after` when it is given). Returns its index.
    int insertParagraph(int game, int ply, int after = -1, Paragraph::Kind kind = Paragraph::Kind::Text);
    /// Moves a paragraph of the game `game` of the open chapter after the
    /// main-line ply `ply` (0: before the first move), first or last among
    /// the paragraphs already there. Returns its new index.
    int moveParagraph(int game, int index, int ply, bool first);
    /// Sets the text of a paragraph; an empty text removes it.
    void setParagraph(int game, int index, const QString &text);
    /// Takes a paragraph, a title or a subtitle away.
    void removeParagraph(int game, int index);

private:
    /// No chapter yet: the one in `chapters` is a holder.
    bool m_none = true;
    /// The chapters came by themselves (isAutomatic).
    bool m_automatic = true;
};
