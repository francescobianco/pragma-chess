#pragma once

#include "app/BoardState.h"

#include <QElapsedTimer>
#include <QWidget>

#include <optional>

class QTimer;

/// The two clocks of a game played online, as a tournament's digital clock:
/// a dark display each, its king for its colour and the time in large
/// seven-segment figures (drawn here, the unlit segments faint behind them),
/// laid out from the left: first the colour at the bottom of the board, as
/// it is turned. The running clock ticks on its own between the platform's
/// updates, which set it right again.
class ChessClocks : public QWidget {
    Q_OBJECT

public:
    explicit ChessClocks(QWidget *parent = nullptr);

    /// The time left to each colour; `running` the clock going, none before the
    /// clocks start or once the game is over.
    void setClocks(int whiteMs, int blackMs, std::optional<Side> running);
    /// Follows the board: the colour at its bottom comes first.
    void setFlipped(bool flipped);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintFace(QPainter &painter, const QRectF &area, Side side, int ms, bool running) const;
    /// How wide a display is: the same for both, from the longer time's shape.
    qreal displayWidth() const;
    int remaining(Side side) const;

    int m_ms[2] = {0, 0}; // By Side.
    std::optional<Side> m_running;
    bool m_flipped = false;
    QElapsedTimer m_since;
    QTimer *m_tick;
};
