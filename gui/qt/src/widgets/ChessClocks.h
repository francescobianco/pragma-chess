#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

/// The two clocks of a game played online, drawn as a tournament's
/// mechanical clock: a large dial each, the hands set so the flag falls at
/// twelve, the red flag lifted by the minute hand in the last minutes, and
/// the time in large figures under the hands. The running clock ticks on
/// its own between the platform's updates, which set it right again.
class ChessClocks : public QWidget {
    Q_OBJECT

public:
    struct Face {
        QString name;
        int rating = 0;
        int ms = 0;
        bool white = true;
    };

    explicit ChessClocks(QWidget *parent = nullptr);

    /// `running`: 0 the left clock, 1 the right one, -1 neither (before the
    /// clocks start, or when the game is over).
    void setClocks(const Face &left, const Face &right, int running);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintFace(QPainter &painter, const QRectF &area, const Face &face, int ms, bool running) const;
    int remaining(int index) const;

    Face m_faces[2];
    int m_running = -1;
    QElapsedTimer m_since;
    QTimer *m_tick;
};
