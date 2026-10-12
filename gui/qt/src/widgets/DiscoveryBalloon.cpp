#include "DiscoveryBalloon.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr int kTip = 8;         // How far the tip comes out of the balloon.
constexpr int kRadius = 8;
constexpr int kTextWidth = 320; // A width that reads: about sixty characters.
constexpr int kPadding = 12;
constexpr int kGap = 2;         // Between the control and the tip.
constexpr int kShadow = 12;     // The transparent margin around the balloon that holds its shadow.
constexpr int kGlow = 10;       // How far the glow reaches around the control.
constexpr int kGlowPeriodMs = 1800;

/// A halo breathing around the control the balloon points at, over it and
/// letting the mouse through: the eye finds the button the text names.
class AnchorGlow : public QWidget {
public:
    explicit AnchorGlow(QWidget *window)
        : QWidget(window)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_clock.start();
        m_timer.setInterval(40);
        connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        // In and out over the period, never quite off.
        const qreal phase = (m_clock.elapsed() % kGlowPeriodMs) / qreal(kGlowPeriodMs);
        const qreal breath = 0.4 + 0.6 * (0.5 - 0.5 * std::cos(phase * 2 * std::numbers::pi));
        const QRectF control = QRectF(rect()).adjusted(kGlow, kGlow, -kGlow, -kGlow);
        const qreal edge = std::max(control.width(), control.height()) / 2;
        QRadialGradient gradient(control.center(), edge + kGlow);
        QColor color = palette().color(QPalette::Highlight);
        color.setAlphaF(0.12 * breath);
        gradient.setColorAt(0, color);
        color.setAlphaF(0.6 * breath);
        gradient.setColorAt(edge / (edge + kGlow), color);
        color.setAlphaF(0);
        gradient.setColorAt(1, color);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), gradient);
    }

private:
    QTimer m_timer;
    QElapsedTimer m_clock;
};

} // namespace

DiscoveryBalloon::DiscoveryBalloon(const QString &text, QWidget *anchor)
    : QWidget(anchor->window())
    , m_anchor(anchor)
    , m_glow(new AnchorGlow(anchor->window()))
    , m_label(new QLabel(text, this))
    , m_dontShow(new QCheckBox(tr("Don't show this again"), this))
    , m_ok(new QPushButton(tr("OK"), this))
    , m_layout(new QVBoxLayout(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAccessibleName(tr("Tip"));
    m_label->setWordWrap(true);
    m_label->setTextFormat(Qt::RichText);
    m_label->setFixedWidth(kTextWidth);
    // The balloon is painted in the tooltip's colours: its texts too.
    for (QWidget *widget : {static_cast<QWidget *>(m_label), static_cast<QWidget *>(m_dontShow)}) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::WindowText, anchor->palette().color(QPalette::ToolTipText));
        widget->setPalette(palette);
    }
    m_ok->setDefault(true);
    connect(m_ok, &QPushButton::clicked, this, &QWidget::close);
    auto *row = new QHBoxLayout;
    row->addWidget(m_dontShow);
    row->addStretch();
    row->addWidget(m_ok);
    m_layout->addWidget(m_label);
    m_layout->addLayout(row);
    // The control moves when the window is laid out again: follow it.
    for (QWidget *widget = anchor; widget; widget = widget->parentWidget())
        widget->installEventFilter(this);
    place();
    m_glow->show();
}

DiscoveryBalloon::~DiscoveryBalloon()
{
    delete m_glow;
}

bool DiscoveryBalloon::dontShowAgain() const
{
    return m_dontShow->isChecked();
}

bool DiscoveryBalloon::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::LayoutRequest:
        if (m_anchor)
            place();
        break;
    case QEvent::Hide:
        // The control gone from sight, the balloon has nothing to point at.
        if (watched == m_anchor)
            close();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

/// Above the control, its tip down, over the board: where the eyes of
/// someone who just moved are. Under it, tip up, only where the window has
/// no room above. The widget is the balloon with its shadow's margin around.
void DiscoveryBalloon::place()
{
    QWidget *top = window();
    if (!m_anchor || !top)
        return;
    const QRect control(m_anchor->mapTo(top, QPoint(0, 0)), m_anchor->size());
    const QRect room = top->contentsRect();
    const int side = kShadow + kPadding;
    m_layout->setContentsMargins(side, side, side, side + kTip);
    adjustSize();
    m_below = control.top() - kGap - (height() - kShadow) < room.top();
    if (m_below) {
        m_layout->setContentsMargins(side, side + kTip, side, side);
        adjustSize();
    }
    const int x = qBound(room.left() + 4 - kShadow, control.center().x() - width() / 2, room.right() - width() + kShadow - 4);
    const int y = m_below ? control.bottom() + kGap - kShadow : control.top() - kGap - height() + kShadow;
    move(x, y);
    m_tipX = control.center().x() - x;
    if (m_glow) {
        m_glow->setGeometry(control.adjusted(-kGlow, -kGlow, kGlow, kGlow));
        m_glow->raise();
    }
    raise();
}

void DiscoveryBalloon::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(kShadow + 0.5, kShadow + (m_below ? kTip : 0) + 0.5, -kShadow - 0.5,
                                               -kShadow - (m_below ? 0 : kTip) - 0.5);
    // The shadow, as the menus have it: darker near the balloon, fading out
    // across the margin and a little lower, as light from above casts it.
    painter.setPen(Qt::NoPen);
    for (int spread = kShadow; spread >= 1; --spread) {
        const qreal fade = 1.0 - qreal(spread) / kShadow;
        painter.setBrush(QColor(0, 0, 0, qRound(2.5 + 12 * fade * fade)));
        painter.drawRoundedRect(box.adjusted(-spread, -spread + 2, spread, spread + 1), kRadius + spread,
                                kRadius + spread);
    }
    QPainterPath path;
    path.addRoundedRect(box, kRadius, kRadius);
    QPainterPath tip;
    const qreal x = qBound(box.left() + kRadius + kTip, qreal(m_tipX), box.right() - kRadius - kTip);
    if (m_below) {
        tip.moveTo(x - kTip, box.top() + 1);
        tip.lineTo(x, box.top() - kTip);
        tip.lineTo(x + kTip, box.top() + 1);
    } else {
        tip.moveTo(x - kTip, box.bottom() - 1);
        tip.lineTo(x, box.bottom() + kTip);
        tip.lineTo(x + kTip, box.bottom() - 1);
    }
    tip.closeSubpath();
    path = path.united(tip);
    const QPalette palette = QApplication::palette();
    painter.setPen(QPen(palette.color(QPalette::Mid), 1));
    painter.setBrush(palette.color(QPalette::ToolTipBase));
    painter.drawPath(path);
}

void DiscoveryBalloon::closeEvent(QCloseEvent *event)
{
    // The halo goes with the balloon, now: the widget itself is deleted later.
    if (m_glow) {
        m_glow->hide();
        m_glow->deleteLater();
    }
    if (!m_closedSaid) {
        m_closedSaid = true;
        Q_EMIT closed(dontShowAgain());
    }
    event->accept();
}
