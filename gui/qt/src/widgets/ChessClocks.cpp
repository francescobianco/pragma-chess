#include "ChessClocks.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace {

constexpr int kLowTimeMs = 20000; // Under it the figures turn red.
constexpr int kTenthsMs = 10000;  // Under it tenths are shown, as on the platforms.
constexpr qreal kDigitHeight = 36;
constexpr qreal kDigitWidth = kDigitHeight * 0.52;
constexpr qreal kStroke = kDigitHeight * 0.115;
constexpr qreal kGap = kDigitHeight * 0.14;   // Between two figures.
constexpr qreal kNarrow = kDigitHeight * 0.22; // A colon's or a point's place.
constexpr qreal kPadding = 12;
constexpr qreal kSpacing = 14; // Between the two clocks.

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

/// Segments a…g of each figure, bit 0 = a (top), clockwise, g the middle.
constexpr quint8 kSegments[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

qreal advance(QChar c)
{
    return c.isDigit() ? kDigitWidth + kGap : kNarrow + kGap;
}

qreal textWidth(const QString &text)
{
    qreal width = 0;
    for (QChar c : text)
        width += advance(c);
    return width - kGap;
}

/// One segment: a long hexagon from `a` to `b`.
QPainterPath segment(QPointF a, QPointF b)
{
    const qreal h = kStroke / 2;
    QPainterPath path;
    if (qFuzzyCompare(a.y(), b.y())) { // Horizontal.
        path.moveTo(a.x(), a.y());
        path.lineTo(a.x() + h, a.y() - h);
        path.lineTo(b.x() - h, b.y() - h);
        path.lineTo(b.x(), b.y());
        path.lineTo(b.x() - h, b.y() + h);
        path.lineTo(a.x() + h, a.y() + h);
    } else { // Vertical.
        path.moveTo(a.x(), a.y());
        path.lineTo(a.x() + h, a.y() + h);
        path.lineTo(b.x() + h, b.y() - h);
        path.lineTo(b.x(), b.y());
        path.lineTo(b.x() - h, b.y() - h);
        path.lineTo(a.x() - h, a.y() + h);
    }
    path.closeSubpath();
    return path;
}

/// A figure at `x` (left) and `top`: lit segments in `lit`, the others in `unlit`.
void paintDigit(QPainter &painter, qreal x, qreal top, int digit, const QColor &lit, const QColor &unlit)
{
    const qreal inset = kStroke * 0.55; // Room for the segments' points to meet.
    const qreal l = x + kStroke / 2, r = x + kDigitWidth - kStroke / 2;
    const qreal t = top + kStroke / 2, m = top + kDigitHeight / 2, b = top + kDigitHeight - kStroke / 2;
    const QPainterPath segments[7] = {
        segment({l + inset * 0.3, t}, {r - inset * 0.3, t}), // a
        segment({r, t + inset * 0.3}, {r, m - inset * 0.3}), // b
        segment({r, m + inset * 0.3}, {r, b - inset * 0.3}), // c
        segment({l + inset * 0.3, b}, {r - inset * 0.3, b}), // d
        segment({l, m + inset * 0.3}, {l, b - inset * 0.3}), // e
        segment({l, t + inset * 0.3}, {l, m - inset * 0.3}), // f
        segment({l + inset * 0.3, m}, {r - inset * 0.3, m}), // g
    };
    const quint8 on = digit >= 0 && digit <= 9 ? kSegments[digit] : 0;
    for (int s = 0; s < 7; ++s)
        painter.fillPath(segments[s], (on >> s) & 1 ? lit : unlit);
}

/// The time as the display shows it: figures, colons and the point of the tenths.
void paintTime(QPainter &painter, qreal x, qreal top, const QString &text, const QColor &lit, const QColor &unlit)
{
    // A slight slant, as on the displays.
    painter.save();
    QTransform slant;
    slant.translate(0, top + kDigitHeight);
    slant.shear(-0.07, 0);
    slant.translate(0, -(top + kDigitHeight));
    painter.setTransform(slant, true);
    const qreal dot = kStroke * 0.95;
    for (QChar c : text) {
        if (c.isDigit()) {
            paintDigit(painter, x, top, c.digitValue(), lit, unlit);
        } else if (c == QLatin1Char(':')) {
            const qreal cx = x + kNarrow / 2;
            painter.setPen(Qt::NoPen);
            painter.setBrush(lit);
            painter.drawRect(QRectF(cx - dot / 2, top + kDigitHeight * 0.30 - dot / 2, dot, dot));
            painter.drawRect(QRectF(cx - dot / 2, top + kDigitHeight * 0.70 - dot / 2, dot, dot));
        } else if (c == QLatin1Char('.')) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(lit);
            painter.drawRect(QRectF(x + kNarrow / 2 - dot / 2, top + kDigitHeight - dot, dot, dot));
        }
        x += advance(c);
    }
    painter.restore();
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
    updateGeometry();
    update();
}

qreal ChessClocks::displayWidth() const
{
    // Wide enough for "8:88.8" — the tenths come under ten seconds — or for
    // hours, so the display never changes width while the time runs down.
    qreal width = textWidth(QStringLiteral("8:88.8"));
    for (const Face &face : m_faces)
        width = qMax(width, textWidth(clockText(face.ms).replace(QLatin1Char('.'), QString())));
    return width + 2 * kPadding;
}

QSize ChessClocks::sizeHint() const
{
    const int name = fontMetrics().height() + 6;
    return {int(2 * displayWidth() + kSpacing) + 2, int(kDigitHeight + 2 * kPadding) + name + 2};
}

QSize ChessClocks::minimumSizeHint() const
{
    return sizeHint();
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
    const qreal width = displayWidth();
    // From the left, as the rest of the panel: the opponent's, then the user's.
    for (int i = 0; i < 2; ++i)
        paintFace(painter, QRectF(1 + i * (width + kSpacing), 1, width, height() - 2), m_faces[i], remaining(i),
                  i == m_running);
}

void ChessClocks::paintFace(QPainter &painter, const QRectF &area, const Face &face, int ms, bool running) const
{
    const QPalette pal = palette();
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor red(0xff, 0x4d, 0x4d);

    // The display: dark on any theme, as the clocks are; the running one ringed.
    const QRectF display(area.left(), area.top(), area.width(), kDigitHeight + 2 * kPadding);
    painter.setPen(QPen(running ? accent : QColor(0x55, 0x55, 0x55), running ? 2.5 : 1.0));
    painter.setBrush(QColor(0x16, 0x16, 0x16));
    painter.drawRoundedRect(display.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    const QColor lit = ms < kLowTimeMs ? red : (running ? QColor(0xf4, 0xf4, 0xf0) : QColor(0xb8, 0xb8, 0xb4));
    const QColor unlit(0xff, 0xff, 0xff, 14);
    // The time right-aligned in the display, as a clock's figures stand.
    const QString text = clockText(ms);
    const qreal x = display.right() - kPadding - textWidth(text);
    paintTime(painter, x, display.top() + kPadding, text, lit, unlit);

    // Who it is, under the display, from its left edge.
    QFont nameFont = font();
    nameFont.setBold(running);
    painter.setFont(nameFont);
    const QColor ink = pal.color(QPalette::WindowText);
    const QColor window = pal.color(QPalette::Window);
    const qreal fade = running ? 0.0 : 0.3;
    painter.setPen(QColor::fromRgbF(ink.redF() * (1 - fade) + window.redF() * fade,
                                    ink.greenF() * (1 - fade) + window.greenF() * fade,
                                    ink.blueF() * (1 - fade) + window.blueF() * fade));
    const QString label = face.rating > 0 ? QStringLiteral("%1 (%2)").arg(face.name).arg(face.rating) : face.name;
    const QRectF nameRect(area.left() + 2, display.bottom() + 4, area.width() - 2, area.bottom() - display.bottom() - 4);
    painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignTop,
                     QFontMetricsF(nameFont).elidedText(label, Qt::ElideRight, nameRect.width()));
}
