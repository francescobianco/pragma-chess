#include "EvaluationBar.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {

const QColor kWhiteSide(0xf2, 0xf2, 0xf2);
const QColor kBlackSide(0x40, 0x40, 0x40);

} // namespace

EvaluationBar::EvaluationBar(QWidget *parent)
    : QWidget(parent)
    , m_animation(new QVariantAnimation(this))
{
    setAccessibleName(tr("Evaluation"));
    m_animation->setDuration(250);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_whiteShare = value.toDouble();
        update();
    });
}

void EvaluationBar::setEvaluation(const std::optional<EngineEvaluation> &evaluation)
{
    m_evaluation = evaluation;
    const double target = evaluation ? evaluation->whiteShare() : 0.5;
    setToolTip(evaluation ? tr("%1 (depth %2)").arg(evaluation->text()).arg(evaluation->depth) : QString());
    setAccessibleDescription(evaluation ? evaluation->text() : QString());

    m_animation->stop();
    m_animation->setStartValue(m_whiteShare);
    m_animation->setEndValue(target);
    m_animation->start();
    update();
}

void EvaluationBar::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    update();
}

QSize EvaluationBar::sizeHint() const
{
    return {26, 200};
}

QSize EvaluationBar::minimumSizeHint() const
{
    return {18, 60};
}

void EvaluationBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = QRectF(rect());
    QPainterPath clip;
    clip.addRoundedRect(bounds, 3, 3);
    painter.setClipPath(clip);

    // White's side is where White's pieces start: the bottom, unless flipped.
    const qreal whiteHeight = bounds.height() * m_whiteShare;
    QRectF whiteRect = bounds;
    QRectF blackRect = bounds;
    if (m_flipped) {
        whiteRect.setBottom(bounds.top() + whiteHeight);
        blackRect.setTop(whiteRect.bottom());
    } else {
        whiteRect.setTop(bounds.bottom() - whiteHeight);
        blackRect.setBottom(whiteRect.top());
    }
    painter.fillRect(blackRect, kBlackSide);
    painter.fillRect(whiteRect, kWhiteSide);

    // Midline marks the balanced position.
    painter.fillRect(QRectF(bounds.left(), bounds.center().y() - 0.5, bounds.width(), 1),
                     QColor(0x80, 0x80, 0x80, 0x90));

    if (!m_evaluation)
        return;

    // The score sits at the end of the side that is ahead.
    const bool whiteAhead = m_evaluation->isMate ? m_evaluation->mating == Side::White
                                                 : m_evaluation->centipawns >= 0;
    const bool atBottom = whiteAhead != m_flipped;
    QFont font = this->font();
    font.setPixelSize(qMax(8, int(width() * 0.36)));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(whiteAhead ? kBlackSide : kWhiteSide);
    const QRectF textRect = bounds.adjusted(0, 4, 0, -4);
    painter.drawText(textRect, Qt::AlignHCenter | (atBottom ? Qt::AlignBottom : Qt::AlignTop),
                     m_evaluation->text());
}
