#pragma once

#include "app/UciEngine.h"

#include <QWidget>

#include <optional>

class QPainter;
class QRectF;
class QVariantAnimation;

/// Vertical evaluation gauge shown next to the board: the White share grows
/// from White's side of the board. Flips together with the board.
class EvaluationBar : public QWidget {
    Q_OBJECT

public:
    explicit EvaluationBar(QWidget *parent = nullptr);

    void setEvaluation(const std::optional<EngineEvaluation> &evaluation);
    void setFlipped(bool flipped);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// The score, at the end of the side that is ahead.
    void paintScore(QPainter &painter, const QRectF &bounds) const;

    std::optional<EngineEvaluation> m_evaluation;
    bool m_flipped = false;
    double m_whiteShare = 0.5;
    QVariantAnimation *m_animation;
};
