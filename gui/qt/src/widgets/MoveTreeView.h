#pragma once

#include <QHash>
#include <QList>
#include <QTextBrowser>

class QTextTable;

class GameSession;

/// The moves of the game on the board as a classic table: the main line in
/// two columns, White and Black, with the move number in front, one move a
/// cell — the whole cell is the move, clicking it goes there and the cell of
/// the move on the board is highlighted. Under the move each variation
/// replaces, a row spanning both columns holds that variation as text, its
/// own variations inline in parentheses; there every move is a link.
/// Pieces are figurines.
class MoveTreeView : public QTextBrowser {
    Q_OBJECT

public:
    explicit MoveTreeView(GameSession *session, QWidget *parent = nullptr);

    /// Where a move is: the line (GameSession::path) and the ply on it.
    struct Place {
        QList<int> path;
        int ply = 0;
        bool isValid() const { return ply > 0; }
    };
    /// The move under `position` (in viewport coordinates), if any.
    Place placeAt(const QPoint &position) const;

Q_SIGNALS:
    /// A move was clicked.
    void moveActivated(const QList<int> &path, int ply);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void rebuild();
    void showCurrent();
    /// The ply of the main-line cell under `position`, 0 if none.
    int cellPlyAt(const QPoint &position) const;
    /// The one table of the document.
    QTextTable *table() const;

    GameSession *m_session;
    QHash<int, int> m_cellPlies; // (row << 2 | column) → ply, main line.
    int m_currentCell = -1;      // The highlighted cell, same key; -1 if the current move is in a variation.
};
