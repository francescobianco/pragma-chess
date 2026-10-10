#pragma once

#include <QList>
#include <QWidget>

#include <optional>

/// The Engine panel's score in a box of two sections: on the left the
/// score, large, with the depth under it; on the right the game's course: a
/// dot for the evaluation after every move, joined by a line, nothing filled,
/// over a marked midline — the balance. White's advantage goes towards
/// White's side of the board (down, unless it is turned). The width holds 20
/// moves; past them every move gets its share of it, so the whole game is
/// always in view, never scrolled.
class EvaluationBox : public QWidget {
    Q_OBJECT

public:
    explicit EvaluationBox(QWidget *parent = nullptr);

    /// The score ("+0.4", "M3", "–") and the depth's text (empty for none).
    void setScore(const QString &score, const QString &depth);
    /// White's share (EngineEvaluation::whiteShare) after each ply, 0 the
    /// start; none where no evaluation is known. `current` is the ply on
    /// the board.
    void setCourse(const QList<std::optional<double>> &shares, int current);
    /// Whether White is at the top of the board.
    void setFlipped(bool flipped);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int sectionWidth() const;

    QString m_score;
    QString m_depth;
    QList<std::optional<double>> m_shares;
    int m_current = 0;
    bool m_flipped = false;
};
