#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QTextBrowser>

class ChapterBook;
class QHeaderView;
class QStandardItemModel;
class QTextEdit;
class QTextTable;

class GameSession;

/// The moves of the chapter as a classic table: each game's main line in
/// two columns, White and Black, with the move number in front, one move a
/// cell — the whole cell is the move, clicking it goes there and the cell of
/// the move on the board is highlighted. Under the move each variation
/// replaces, a row spanning both columns holds that variation as text, its
/// own variations inline in parentheses; there every move is a link.
/// Paragraphs are rows of text between the moves, written in place: a click
/// on one opens it for writing right there. In a chapter of several games a
/// light rule marks where each begins, and the numbering starts again.
/// Pieces are figurines. The header is a real QHeaderView over the text, so
/// it looks like the other tables', and its sections set the columns' widths.
class MoveTreeView : public QTextBrowser {
    Q_OBJECT

public:
    explicit MoveTreeView(GameSession *session, QWidget *parent = nullptr);

    /// The chapter around the game on the board (the session is its current
    /// game); without one, the game alone is shown.
    void setBook(const ChapterBook *book);
    /// Shows the chapter again: its games or paragraphs changed.
    void refresh();

    /// What is under a point: a move (the game of the chapter, the line —
    /// GameSession::path — and the ply on it) or a paragraph. `game` is -1
    /// where there is nothing.
    struct Place {
        int game = -1;
        QList<int> path;
        int ply = 0;
        int paragraph = -1;
        bool isMove() const { return ply > 0; }
        bool isParagraph() const { return paragraph >= 0; }
    };
    /// The place under `position` (in viewport coordinates).
    Place placeAt(const QPoint &position) const;

    /// Opens a paragraph of the game `game` for writing, where it is.
    void editParagraph(int game, int index);
    /// Ends the writing, handing the text over (paragraphEdited).
    void finishEditing();

Q_SIGNALS:
    /// A move of the game on the board was clicked.
    void moveActivated(const QList<int> &path, int ply);
    /// A move of another game of the chapter was clicked.
    void gameMoveActivated(int game, const QList<int> &path, int ply);
    /// A paragraph was written: its new text, empty to take it away.
    void paragraphEdited(int game, int index, const QString &text);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void showCurrent();
    /// The table cell under `position`, as (row << 2 | column), or -1.
    int cellAt(const QPoint &position) const;
    /// The one table of the document.
    QTextTable *table() const;
    int currentGame() const;
    /// Lays the editor over the row of the paragraph being written.
    void placeEditor();
    /// Sets every line of the editor as the view sets a paragraph.
    void formatEditor();

    GameSession *m_session;
    const ChapterBook *m_book = nullptr;
    QHeaderView *m_header;         // The classic header above the text, as the other tables have.
    QStandardItemModel *m_columns; // Its titles.
    QHash<int, QPair<int, int>> m_cellPlaces; // (row << 2 | column) → (game, ply), main lines.
    QHash<int, QPair<int, int>> m_paragraphRows; // row → (game, paragraph).
    int m_currentCell = -1; // The highlighted cell, same key; -1 if the current move is in a variation.

    /// rebuild() is running: setHtml() can resize the view, whose header asks
    /// for another rebuild from inside it; that one waits (m_rebuildAgain).
    bool m_rebuilding = false;
    bool m_rebuildAgain = false;

    QTextEdit *m_editor;
    /// The paragraph being written, (game, index), and its text so far.
    QPair<int, int> m_editing{-1, -1};
    QString m_editText;
    bool m_formatting = false;
};
