#include "EvaluationBox.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {

/// The width holds this many plies (20 moves) before they share it.
constexpr int kPliesShown = 40;
constexpr int kPadding = 10;

} // namespace

EvaluationBox::EvaluationBox(QWidget *parent)
    : QWidget(parent)
    , m_score(QStringLiteral("–"))
{
    setAccessibleName(tr("Evaluation"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true); // The dot under the pointer lights up.
}

void EvaluationBox::setScore(const QString &score, const QString &depth)
{
    if (score == m_score && depth == m_depth)
        return;
    m_score = score;
    m_depth = depth;
    setAccessibleDescription(depth.isEmpty() ? score : score + QLatin1String(", ") + depth);
    updateGeometry();
    update();
}

void EvaluationBox::setCourse(const QList<std::optional<double>> &shares, int current)
{
    m_shares = shares;
    m_current = current;
    update();
}

void EvaluationBox::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    update();
}

namespace {

QFont scoreFont(const QFont &base)
{
    QFont font = base;
    font.setPointSizeF(font.pointSizeF() * 1.8);
    font.setBold(true);
    return font;
}

QFont depthFont(const QFont &base)
{
    QFont font = base;
    font.setPointSizeF(font.pointSizeF() * 0.85);
    return font;
}

} // namespace

QRectF EvaluationBox::graphArea() const
{
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const int section = sectionWidth();
    return {qreal(section), bounds.top(), bounds.right() - section, bounds.height()};
}

QList<std::pair<int, QPointF>> EvaluationBox::dots() const
{
    const QRectF graph = graphArea();
    const int plies = int(m_shares.size()) - 1;
    const qreal step = graph.width() / qMax(kPliesShown, plies);
    // The dots stay whole at the top and the bottom.
    const qreal inset = 4;
    QList<std::pair<int, QPointF>> found;
    for (int ply = 0; ply <= plies; ++ply) {
        std::optional<double> share = m_shares.at(ply);
        if (!share && ply == 0)
            share = 0.5; // The start is even until known otherwise.
        if (!share)
            continue;
        // White's side is where White's pieces start: the bottom, unless turned.
        const qreal y = m_flipped ? graph.top() + *share * graph.height() : graph.bottom() - *share * graph.height();
        found.push_back({ply, QPointF(graph.left() + ply * step, qBound(graph.top() + inset, y, graph.bottom() - inset))});
    }
    return found;
}

int EvaluationBox::plyAt(const QPointF &position) const
{
    // The nearest dot across, within reach: crowded dots are still each one's.
    const QList<std::pair<int, QPointF>> points = dots();
    if (points.size() < 2 || !graphArea().contains(position))
        return -1;
    int nearest = -1;
    qreal best = 8;
    for (const auto &[ply, point] : points) {
        const qreal distance = std::abs(point.x() - position.x());
        if (distance < best) {
            best = distance;
            nearest = ply;
        }
    }
    return nearest;
}

void EvaluationBox::mouseMoveEvent(QMouseEvent *event)
{
    const int ply = plyAt(event->position());
    if (ply == m_hovered)
        return;
    m_hovered = ply;
    setCursor(ply >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void EvaluationBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return QWidget::mousePressEvent(event);
    if (const int ply = plyAt(event->position()); ply >= 0)
        Q_EMIT plyClicked(ply);
}

void EvaluationBox::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

int EvaluationBox::sectionWidth() const
{
    // As wide as the widest score it may show, so it never moves.
    const QFontMetrics score(scoreFont(font()));
    const QFontMetrics depth(depthFont(font()));
    const int widest = qMax(score.horizontalAdvance(QStringLiteral("−88.8")),
                            depth.horizontalAdvance(tr("Depth %1").arg(88)));
    return widest + 2 * kPadding;
}

QSize EvaluationBox::sizeHint() const
{
    const QFontMetrics score(scoreFont(font()));
    const QFontMetrics depth(depthFont(font()));
    return {sectionWidth() + 200, score.height() + depth.height() + kPadding};
}

QSize EvaluationBox::minimumSizeHint() const
{
    return {sectionWidth() + 60, sizeHint().height()};
}

void EvaluationBox::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = 4;
    QColor edge = palette().color(QPalette::WindowText);
    edge.setAlphaF(0.28);

    // The score's section: the score, the depth under it.
    const int section = sectionWidth();
    const QFontMetrics scoreMetrics(scoreFont(font()));
    const QFontMetrics depthMetrics(depthFont(font()));
    const int textHeight = scoreMetrics.height() + (m_depth.isEmpty() ? 0 : depthMetrics.height());
    const int top = (height() - textHeight) / 2;
    painter.setFont(scoreFont(font()));
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(QRect(0, top, section, scoreMetrics.height()), Qt::AlignCenter, m_score);
    if (!m_depth.isEmpty()) {
        painter.setFont(depthFont(font()));
        painter.setPen(palette().color(QPalette::Disabled, QPalette::WindowText));
        painter.drawText(QRect(0, top + scoreMetrics.height(), section, depthMetrics.height()), Qt::AlignCenter, m_depth);
    }

    // The course of the game, clipped to the box's rounded right end.
    const QRectF graph = graphArea();
    QPainterPath box;
    box.addRoundedRect(bounds, radius, radius);
    QPainterPath clipArea;
    clipArea.addRect(graph);
    painter.save();
    painter.setClipPath(box.intersected(clipArea));
    const int plies = int(m_shares.size()) - 1;
    const qreal step = graph.width() / qMax(kPliesShown, plies);

    // The midline, the balance, marked: above it one side is better, below it the other.
    QColor midline = palette().color(QPalette::WindowText);
    midline.setAlphaF(0.45);
    painter.setPen(QPen(midline, 1.5));
    painter.drawLine(QPointF(graph.left(), graph.center().y()), QPointF(graph.right(), graph.center().y()));
    // The move on the board.
    if (m_current >= 0 && m_current <= plies && plies > 0) {
        const qreal x = graph.left() + m_current * step;
        QColor marker = palette().color(QPalette::Highlight);
        marker.setAlphaF(0.6);
        painter.setPen(QPen(marker, 1.5));
        painter.drawLine(QPointF(x, graph.top()), QPointF(x, graph.bottom()));
    }

    // A dot for each move whose evaluation is known, joined to the next one
    // known by a line: nothing filled.
    const QList<std::pair<int, QPointF>> points = dots();
    const QColor ink = palette().color(QPalette::WindowText);
    if (points.size() >= 2) {
        QList<QPointF> line;
        for (const auto &[ply, point] : points)
            line << point;
        painter.setPen(QPen(ink, 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(line.constData(), int(line.size()));
    }
    // Small enough not to touch when the moves crowd the width.
    const qreal dot = qBound(1.2, step / 3, 2.6);
    for (const auto &[ply, point] : points) {
        const bool marked = ply == m_current || ply == m_hovered;
        painter.setPen(Qt::NoPen);
        painter.setBrush(marked ? palette().color(QPalette::Highlight) : ink);
        painter.drawEllipse(point, marked ? dot + 1.2 : dot, marked ? dot + 1.2 : dot);
    }
    painter.restore();

    // The line between the sections, and the frame, as the evaluation bar's.
    painter.setPen(QPen(edge, 1));
    painter.drawLine(QPointF(section + 0.5, bounds.top()), QPointF(section + 0.5, bounds.bottom()));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bounds, radius, radius);
}
