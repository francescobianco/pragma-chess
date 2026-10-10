#pragma once

#include "app/ScoreView.h"

#include <QList>
#include <QWidget>

#include <optional>

/// The Engine panel's score in a box of two sections: on the left the
/// score, large, with what it is and the depth under it — a click there
/// shows it another way (ScoreView: absolute, for the colour at the bottom,
/// its chances, the book's symbol), four dots saying which; on the right the
/// game's course: a
/// dot for the evaluation after every move, joined by a line, nothing filled,
/// over a marked midline — the balance. White's advantage goes towards
/// White's side of the board (down, unless it is turned). The width holds 20
/// moves; past them every move gets its share of it, so the whole game is
/// always in view, never scrolled.
class EvaluationBox : public QWidget {
    Q_OBJECT

public:
    explicit EvaluationBox(QWidget *parent = nullptr);

    /// The evaluation shown; none shows a dash.
    void setEvaluation(const std::optional<EngineEvaluation> &evaluation);
    void setView(ScoreView::Kind view);
    ScoreView::Kind view() const { return m_view; }
    /// Where each ply sits, 0 the start: 1 White's edge, 0 Black's, 0.5 the
    /// balance (ScoreView::courseHeight, logarithmic); none where no
    /// evaluation is known. `current` is the ply on
    /// the board.
    void setCourse(const QList<std::optional<double>> &shares, int current);
    /// Whether White is at the top of the board.
    void setFlipped(bool flipped);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// A dot was clicked: the board goes to that ply of the line.
    void plyClicked(int ply);
    /// The score was clicked: it is shown another way, to be remembered.
    void viewChanged(ScoreView::Kind view);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int sectionWidth() const;
    /// Where the graph is drawn.
    QRectF graphArea() const;
    /// The dots: each ply known and where it is drawn.
    QList<std::pair<int, QPointF>> dots() const;
    /// The ply of the dot under `position`, or -1.
    int plyAt(const QPointF &position) const;

    /// Whether `position` is on the score's section.
    bool onScore(const QPointF &position) const;
    void updateToolTip();

    std::optional<EngineEvaluation> m_evaluation;
    ScoreView::Kind m_view = ScoreView::Kind::Absolute;
    bool m_hoverScore = false;
    QString m_scoreTip;
    QList<std::optional<double>> m_shares;
    int m_current = 0;
    int m_hovered = -1;
    bool m_flipped = false;
};
