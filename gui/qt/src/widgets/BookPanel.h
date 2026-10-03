#pragma once

#include "app/OpeningNames.h"
#include "app/PolyglotBook.h"
#include "app/PositionIndex.h"

#include <QWidget>

class QTreeWidget;

/// Contents of the Opening Tree dock: the moves the chosen book plays in the
/// position on the board, with the name of the opening each move leads to and
/// how the games of the open database reaching it ended, and its share of the
/// book's weight. Moves in the user's repertoire come first,
/// in bold and brighter; a right click puts a move in or takes it out. The first
/// row always leads back up the tree, one move back. The current book and opening are shown in the Engine panel.
class BookPanel : public QWidget {
    Q_OBJECT

public:
    explicit BookPanel(QWidget *parent = nullptr);

    /// The name of the chosen book, empty when no book is chosen (for the empty state).
    void setBookName(const QString &name);
    /// The book moves of `position`, heaviest first, with the name of the
    /// position each one leads to (`names` matches `moves`, names may be empty).
    /// `lastMove` is the move the first row takes back ("4…♘f6"), empty at the start.
    void setMoves(const ChessPosition &position, const QList<PolyglotBook::Move> &moves,
                  const QList<OpeningNames::Name> &names, const QString &lastMove);

    /// What the Database column can say about the open database.
    enum class DatabaseState { NoDatabase, Indexing, Ready };
    /// The games of the open database reaching the position after each book
    /// move (`stats` matches the moves given to setMoves when Ready).
    void setDatabaseStats(DatabaseState state, const QList<PositionIndex::Stats> &stats);

Q_SIGNALS:
    /// The user picked a book move to play.
    void moveActivated(const ChessMove &move);
    /// The user asked to go back one move, up the tree.
    void backActivated();
    /// The user put a book move in their repertoire or took it out (right-click menu).
    void repertoireToggled(const ChessMove &move, bool inRepertoire);
    /// The user asked to change a move's weight by `percent` of its share,
    /// or, with 0, to take it all away (BookWeights).
    void weightAdjustRequested(const ChessMove &move, int percent);

private:
    void rebuild();

    QTreeWidget *m_moves;
    QString m_bookName;
    ChessPosition m_position = ChessPosition::startingPosition();
    QList<PolyglotBook::Move> m_bookMoves;
    QList<OpeningNames::Name> m_names;
    QString m_lastMove;
    DatabaseState m_databaseState = DatabaseState::NoDatabase;
    QList<PositionIndex::Stats> m_stats;
    static constexpr int kDatabaseColumn = 2;
    static constexpr int kWeightColumn = 3;
    static constexpr int kColumns = 4;
    /// The rows of book moves start after the back row.
    static constexpr int kFirstMoveRow = 1;
};
