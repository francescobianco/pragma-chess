#pragma once

#include "app/OpeningNames.h"
#include "app/PolyglotBook.h"

#include <QWidget>

class QLabel;
class QStackedWidget;
class QTreeWidget;

/// Contents of the Opening Tree dock: the moves the chosen book plays in the
/// position on the board, with the name of the opening each move leads to and
/// its share of the book's weight. The current book and opening are shown in
/// the Engine panel.
class BookPanel : public QWidget {
    Q_OBJECT

public:
    explicit BookPanel(QWidget *parent = nullptr);

    /// The name of the chosen book, empty when no book is chosen (for the empty state).
    void setBookName(const QString &name);
    /// The book moves of `position`, heaviest first, with the name of the
    /// position each one leads to (`names` matches `moves`, names may be empty).
    void setMoves(const ChessPosition &position, const QList<PolyglotBook::Move> &moves,
                  const QList<OpeningNames::Name> &names);

Q_SIGNALS:
    /// The user picked a book move to play.
    void moveActivated(const ChessMove &move);

private:
    void updateMessage();

    QStackedWidget *m_stack;
    QLabel *m_message;
    QTreeWidget *m_moves;
    QString m_bookName;
    QList<ChessMove> m_shownMoves;
};
