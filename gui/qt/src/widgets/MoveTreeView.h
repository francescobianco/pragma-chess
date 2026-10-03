#pragma once

#include <QList>
#include <QTextBrowser>

class GameSession;

/// The moves of the game on the board as a tree: the main line in two
/// columns, White and Black, with the move number in front, and under the
/// move each variation replaces a block with that variation, its own
/// variations inline in parentheses. Clicking a move goes there, in whatever
/// line it is; the move on the board is highlighted. Pieces are figurines.
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

private:
    void rebuild();
    void showCurrent();

    GameSession *m_session;
};
