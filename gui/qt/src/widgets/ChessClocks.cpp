#include "ChessClocks.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

namespace {

constexpr int kLowTimeMs = 20000;   // Under it the figures turn red.
constexpr int kTenthsMs = 10000;    // Under it tenths are shown, as on the platforms.
constexpr qreal kFlagMinutes = 3.0; // The minute hand lifts the flag over the last three minutes.

QColor mix(const QColor &a, const QColor &b, qreal share)
{
    return QColor::fromRgbF(a.redF() * (1 - share) + b.redF() * share, a.greenF() * (1 - share) + b.greenF() * share,
                            a.blueF() * (1 - share) + b.blueF() * share);
}

QString clockText(int ms)
{
    ms = qMax(0, ms);
    const int seconds = ms / 1000;
    if (seconds >= 3600)
        return QStringLiteral("%1:%2:%3")
            .arg(seconds / 3600)
            .arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    QString text = QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    if (ms < kTenthsMs)
        text += QStringLiteral(".%1").arg(ms / 100 % 10);
    return text;
}

} // namespace

ChessClocks::ChessClocks(QWidget *parent)
    : QWidget(parent)
    , m_tick(new QTimer(this))
{
    m_tick->setInterval(100);
    connect(m_tick, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ChessClocks::setClocks(const Face &left, const Face &right, int running)
{
    m_faces[0] = left;
    m_faces[1] = right;
    m_running = running;
    m_since.start();
    if (running >= 0)
        m_tick->start();
    else
        m_tick->stop();
    update();
}

QSize ChessClocks::sizeHint() const
{
    return {320, 210};
}

QSize ChessClocks::minimumSizeHint() const
{
    return {220, 210};
}

int ChessClocks::remaining(int index) const
{
    const int ms = m_faces[index].ms;
    return index == m_running ? int(qMax<qint64>(0, ms - m_since.elapsed())) : ms;
}

void ChessClocks::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal gap = 12;
    const qreal half = (width() - gap) / 2.0;
    for (int i = 0; i < 2; ++i)
        paintFace(painter, QRectF(i * (half + gap), 0, half, height()), m_faces[i], remaining(i), i == m_running);
}

void ChessClocks::paintFace(QPainter &painter, const QRectF &area, const Face &face, int ms, bool running) const
{
    const QPalette pal = palette();
    const QColor ink = pal.color(QPalette::WindowText);
    const QColor window = pal.color(QPalette::Window);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor red(0xd0, 0x2b, 0x2b);

    // The name and rating under the dial, as the label on the clock's case.
    QFont nameFont = font();
    nameFont.setBold(running);
    const qreal nameHeight = QFontMetricsF(nameFont).height() + 4;
    const qreal side = qMin(area.width(), area.height() - nameHeight) - 4;
    const QRectF dial(area.center().x() - side / 2, area.top() + 2, side, side);
    const QPointF centre = dial.center();
    const qreal radius = side / 2;

    // The case and the face: the running one lit by the selection's colour.
    painter.setPen(QPen(running ? accent : mix(ink, window, 0.6), running ? 3.0 : 1.5));
    painter.setBrush(face.white ? QColor(0xfb, 0xf8, 0xf0) : QColor(0xf1, 0xec, 0xe2));
    painter.drawEllipse(dial.adjusted(1.5, 1.5, -1.5, -1.5));
    const QColor dialInk(0x22, 0x22, 0x22); // The face is paper on any theme.

    // Sixty minute marks, the five-minute ones longer, and the hours' figures.
    for (int m = 0; m < 60; ++m) {
        const qreal angle = qDegreesToRadians(m * 6.0);
        const qreal outer = radius - 6;
        const qreal inner = outer - (m % 5 == 0 ? radius * 0.12 : radius * 0.05);
        painter.setPen(QPen(dialInk, m % 5 == 0 ? 2.0 : 1.0));
        painter.drawLine(QPointF(centre.x() + inner * qSin(angle), centre.y() - inner * qCos(angle)),
                         QPointF(centre.x() + outer * qSin(angle), centre.y() - outer * qCos(angle)));
    }
    QFont figures = font();
    figures.setPixelSize(qMax(8, int(radius * 0.15)));
    painter.setFont(figures);
    painter.setPen(dialInk);
    for (int h = 1; h <= 12; ++h) {
        const qreal angle = qDegreesToRadians(h * 30.0);
        const qreal at = radius * 0.68;
        const QPointF p(centre.x() + at * qSin(angle), centre.y() - at * qCos(angle));
        painter.drawText(QRectF(p.x() - radius * 0.15, p.y() - radius * 0.12, radius * 0.3, radius * 0.24),
                         Qt::AlignCenter, QString::number(h));
    }

    // The flag at twelve: lifted by the minute hand over the last minutes, fallen at zero.
    const qreal minutesLeft = ms / 60000.0;
    qreal lift = 0; // 0 hanging, 1 at its highest.
    if (ms <= 0)
        lift = 0;
    else if (minutesLeft < kFlagMinutes)
        lift = 1.0 - minutesLeft / kFlagMinutes;
    {
        painter.save();
        // Pivoting just right of twelve, above the figure, among the marks.
        const QPointF pivot(centre.x() + radius * 0.07, centre.y() - radius * 0.88);
        painter.translate(pivot);
        painter.rotate(ms <= 0 ? -90 : -35 + lift * 35); // Hanging down-left, lifted level, fallen once the time is out.
        QPainterPath flag;
        flag.moveTo(0, 0);
        flag.lineTo(-radius * 0.24, -radius * 0.05);
        flag.lineTo(-radius * 0.21, radius * 0.08);
        flag.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(red);
        painter.drawPath(flag);
        painter.setBrush(QColor(0x22, 0x22, 0x22));
        painter.drawEllipse(QPointF(0, 0), radius * 0.025, radius * 0.025); // Its pin.
        painter.restore();
    }

    // The large figures, under the hands' centre.
    QFont digits = font();
    digits.setPixelSize(qMax(12, int(radius * 0.30)));
    digits.setBold(true);
    digits.setStyleHint(QFont::Monospace);
    painter.setFont(digits);
    painter.setPen(ms < kLowTimeMs ? red : dialInk);
    painter.drawText(QRectF(dial.left(), centre.y() + radius * 0.14, dial.width(), radius * 0.42), Qt::AlignCenter,
                     clockText(ms));

    // The hands, set so that twelve is the end: they show twelve minus what is left.
    const qreal hoursLeft = ms / 3600000.0;
    const auto hand = [&](qreal turns, qreal length, qreal width, const QColor &colour) {
        const qreal angle = qDegreesToRadians(-turns * 360.0);
        painter.setPen(QPen(colour, width, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(centre, QPointF(centre.x() + length * qSin(angle), centre.y() - length * qCos(angle)));
    };
    hand(hoursLeft / 12.0, radius * 0.45, qMax(2.5, radius * 0.06), dialInk);
    hand(minutesLeft / 60.0, radius * 0.78, qMax(2.0, radius * 0.04), dialInk);
    if (running)
        hand(ms / 60000.0 - qFloor(ms / 60000.0), radius * 0.82, 1.2, red); // The seconds, while it runs.
    painter.setPen(Qt::NoPen);
    painter.setBrush(dialInk);
    painter.drawEllipse(centre, radius * 0.05, radius * 0.05);

    // Who it is.
    painter.setFont(nameFont);
    painter.setPen(running ? ink : mix(ink, window, 0.35));
    const QString label = face.rating > 0 ? QStringLiteral("%1 (%2)").arg(face.name).arg(face.rating) : face.name;
    const QRectF nameRect(area.left(), dial.bottom() + 4, area.width(), nameHeight);
    painter.drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop,
                     QFontMetricsF(nameFont).elidedText(label, Qt::ElideRight, area.width()));
}
