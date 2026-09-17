#pragma once

#include "app/OpeningNames.h"
#include "app/PolyglotBook.h"

#include <QWidget>

class QTreeWidget;

/// Contents of the Opening Tree dock: the moves the chosen book plays in the
/// position on the board, with the name of the opening each move leads to and
/// its share of the book's weight. The first row always leads back up the tree,
/// one move back. The current book and opening are shown in the Engine panel.
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

Q_SIGNALS:
    /// The user picked a book move to play.
    void moveActivated(const ChessMove &move);
    /// The user asked to go back one move, up the tree.
    void backActivated();

private:
    void rebuild();

    QTreeWidget *m_moves;
    QString m_bookName;
    ChessPosition m_position = ChessPosition::startingPosition();
    QList<PolyglotBook::Move> m_bookMoves;
    QList<OpeningNames::Name> m_names;
    QString m_lastMove;
    /// The rows of book moves start after the back row.
    static constexpr int kFirstMoveRow = 1;
};
