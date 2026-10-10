#include "ChessClocks.h"

#include "widgets/PieceRenderer.h"

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
constexpr qreal kKing = kDigitHeight; // The king's square, as tall as the figures.

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

void ChessClocks::setClocks(int whiteMs, int blackMs, std::optional<Side> running)
{
    m_ms[int(Side::White)] = whiteMs;
    m_ms[int(Side::Black)] = blackMs;
    m_running = running;
    m_since.start();
    if (running)
        m_tick->start();
    else
        m_tick->stop();
    updateGeometry();
    update();
}

void ChessClocks::setFlipped(bool flipped)
{
    m_flipped = flipped;
    update();
}

qreal ChessClocks::displayWidth() const
{
    // Wide enough for "8:88.8" — the tenths come under ten seconds — or for
    // hours, so the display never changes width while the time runs down.
    qreal width = textWidth(QStringLiteral("8:88.8"));
    for (int ms : m_ms)
        width = qMax(width, textWidth(clockText(ms).replace(QLatin1Char('.'), QString())));
    return width + kKing + 3 * kPadding;
}

QSize ChessClocks::sizeHint() const
{
    return {int(2 * displayWidth() + kSpacing) + 2, int(kDigitHeight + 2 * kPadding) + 2};
}

QSize ChessClocks::minimumSizeHint() const
{
    return sizeHint();
}

int ChessClocks::remaining(Side side) const
{
    const int ms = m_ms[int(side)];
    return side == m_running ? int(qMax<qint64>(0, ms - m_since.elapsed())) : ms;
}

void ChessClocks::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal width = displayWidth();
    // From the left, as the rest of the panel: the colour at the board's bottom first.
    const Side first = m_flipped ? Side::Black : Side::White;
    const Side sides[2] = {first, first == Side::White ? Side::Black : Side::White};
    for (int i = 0; i < 2; ++i)
        paintFace(painter, QRectF(1 + i * (width + kSpacing), 1, width, height() - 2), sides[i], remaining(sides[i]),
                  sides[i] == m_running);
}

void ChessClocks::paintFace(QPainter &painter, const QRectF &area, Side side, int ms, bool running) const
{
    const QColor accent = palette().color(QPalette::Highlight);
    const QColor red(0xff, 0x4d, 0x4d);

    // The display: dark on any theme, as the clocks are; the running one ringed.
    const QRectF display(area.left(), area.top(), area.width(), kDigitHeight + 2 * kPadding);
    painter.setPen(QPen(running ? accent : QColor(0x55, 0x55, 0x55), running ? 2.5 : 1.0));
    painter.setBrush(QColor(0x16, 0x16, 0x16));
    painter.drawRoundedRect(display.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    // Its colour: the king, on a square of paper so the black one shows on the dark.
    const QRectF square(display.left() + kPadding, display.top() + kPadding, kKing, kKing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xee, 0xe8, 0xd8));
    painter.drawRoundedRect(square, 4, 4);
    PieceRenderer::paint(painter, Piece{PieceType::King, side}, square.adjusted(2, 2, -2, -2), devicePixelRatioF());

    const QColor lit = ms < kLowTimeMs ? red : (running ? QColor(0xf4, 0xf4, 0xf0) : QColor(0xb8, 0xb8, 0xb4));
    const QColor unlit(0xff, 0xff, 0xff, 14);
    // The time right-aligned in the display, as a clock's figures stand.
    const QString text = clockText(ms);
    const qreal x = display.right() - kPadding - textWidth(text);
    paintTime(painter, x, display.top() + kPadding, text, lit, unlit);
}
