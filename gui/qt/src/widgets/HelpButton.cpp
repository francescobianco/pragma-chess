#include "HelpButton.h"

#include <QApplication>
#include <QEnterEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kTip = 8;         // How far the tip comes out of the balloon.
constexpr int kRadius = 8;
constexpr int kTextWidth = 320; // A width that reads: about sixty characters.
constexpr int kPadding = 12;

/// The balloon: the text in a rounded box, its tip pointing at the button.
class Bubble : public QWidget {
public:
    Bubble(const QString &text, QWidget *anchor)
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint)
        , m_label(new QLabel(text, this))
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_DeleteOnClose);
        m_label->setWordWrap(true);
        m_label->setTextFormat(Qt::RichText);
        m_label->setFixedWidth(kTextWidth);
        QPalette palette = m_label->palette();
        palette.setColor(QPalette::WindowText, anchor->palette().color(QPalette::ToolTipText));
        m_label->setPalette(palette);
        m_layout = new QVBoxLayout(this);
        m_layout->addWidget(m_label);
        place(anchor);
    }

private:
    /// Below the button, its tip up; above it, tip down, where there is no room below.
    void place(QWidget *anchor)
    {
        const QRect button(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
        const QRect screen = anchor->screen() ? anchor->screen()->availableGeometry() : QRect();
        m_layout->setContentsMargins(kPadding, kPadding + kTip, kPadding, kPadding);
        adjustSize();
        m_below = screen.isNull() || button.bottom() + height() < screen.bottom();
        if (!m_below) {
            m_layout->setContentsMargins(kPadding, kPadding, kPadding, kPadding + kTip);
            adjustSize();
        }
        int x = button.center().x() - width() / 2;
        if (!screen.isNull())
            x = qBound(screen.left() + 4, x, screen.right() - width() - 4);
        const int y = m_below ? button.bottom() + 2 : button.top() - height() - 2;
        move(x, y);
        m_tipX = button.center().x() - x;
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(rect()).adjusted(0.5, m_below ? kTip + 0.5 : 0.5, -0.5, m_below ? -0.5 : -kTip - 0.5);
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

    QLabel *m_label;
    QVBoxLayout *m_layout = nullptr;
    bool m_below = true;
    int m_tipX = 0;
};

} // namespace

HelpButton::HelpButton(const QString &text, QWidget *parent)
    : QToolButton(parent)
    , m_text(text)
    , m_delay(new QTimer(this))
{
    setText(QStringLiteral("?"));
    setAutoRaise(true);
    setFocusPolicy(Qt::TabFocus);
    setAccessibleName(tr("Help"));
    setFixedSize(22, 22);
    m_delay->setSingleShot(true);
    m_delay->setInterval(250);
    connect(m_delay, &QTimer::timeout, this, &HelpButton::showBubble);
    connect(this, &QToolButton::clicked, this, [this] { m_bubble ? hideBubble() : showBubble(); });
}

namespace {

/// Keeps a "?" beside a tall field with its middle on the middle line of
/// the form's label for that row, whatever the font: the label is placed by
/// QFormLayout, so the "?" is moved down by a spacer each time the label or
/// the row moves.
class LabelFollower : public QObject {
public:
    LabelFollower(QWidget *row, QWidget *button, QSpacerItem *spacer)
        : QObject(row), m_row(row), m_button(button), m_spacer(spacer)
    {
        row->installEventFilter(this);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const QEvent::Type type = event->type();
        if (type == QEvent::Show || type == QEvent::Move || type == QEvent::Resize)
            follow();
        return QObject::eventFilter(watched, event);
    }

private:
    void follow()
    {
        if (!m_label) {
            // The form whose row this is, and that row's label.
            QWidget *parent = m_row->parentWidget();
            for (QFormLayout *form : parent ? parent->findChildren<QFormLayout *>() : QList<QFormLayout *>()) {
                if (QWidget *label = form->labelForField(m_row)) {
                    m_label = label;
                    label->installEventFilter(this);
                    break;
                }
            }
            if (!m_label)
                return;
        }
        // Label and row are siblings: the same coordinates.
        const int offset = std::max(0, m_label->geometry().center().y() - m_row->y() - m_button->height() / 2);
        if (offset == m_offset)
            return;
        m_offset = offset;
        m_spacer->changeSize(0, offset, QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_row->layout()->invalidate();
    }

    QWidget *m_row;
    QWidget *m_button;
    QSpacerItem *m_spacer;
    QPointer<QWidget> m_label;
    int m_offset = 0;
};

} // namespace

QWidget *HelpButton::beside(QWidget *field, const QString &help, QWidget *parent, Qt::Alignment alignment)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(field, 1);
    auto *button = new HelpButton(help, row);
    if (alignment & Qt::AlignTop) {
        auto *column = new QVBoxLayout;
        auto *spacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        column->addItem(spacer);
        column->addWidget(button);
        column->addStretch();
        layout->addLayout(column);
        new LabelFollower(row, button, spacer);
    } else {
        layout->addWidget(button, 0, alignment);
    }
    return row;
}

HelpButton::~HelpButton()
{
    hideBubble();
}

void HelpButton::setHelpText(const QString &text)
{
    m_text = text;
}

void HelpButton::enterEvent(QEnterEvent *event)
{
    QToolButton::enterEvent(event);
    m_delay->start();
}

void HelpButton::leaveEvent(QEvent *event)
{
    QToolButton::leaveEvent(event);
    m_delay->stop();
    hideBubble();
}

void HelpButton::hideEvent(QHideEvent *event)
{
    QToolButton::hideEvent(event);
    hideBubble();
}

void HelpButton::paintEvent(QPaintEvent *)
{
    // A "?" in a thin circle, as help reads on the desktop.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // Muted, and the same hovered or not: a remark beside the field, not a control
    // calling for attention. The text's colour halfway to the window's.
    const QColor text = palette().color(isEnabled() ? QPalette::Normal : QPalette::Disabled, QPalette::WindowText);
    const QColor window = palette().color(QPalette::Window);
    const qreal share = 0.55;
    const QColor color = QColor::fromRgbF(text.redF() * (1 - share) + window.redF() * share,
                                          text.greenF() * (1 - share) + window.greenF() * share,
                                          text.blueF() * (1 - share) + window.blueF() * share);
    const qreal side = qMin(width(), height()) - 4.0;
    const QRectF circle((width() - side) / 2, (height() - side) / 2, side, side);
    painter.setPen(QPen(color, 1.2));
    painter.drawEllipse(circle);
    QFont small = font();
    small.setBold(true);
    small.setPixelSize(qRound(side * 0.62));
    painter.setFont(small);
    painter.drawText(circle, Qt::AlignCenter, QStringLiteral("?"));
}

void HelpButton::showBubble()
{
    if (m_bubble || m_text.isEmpty() || !isVisible())
        return;
    m_bubble = new Bubble(m_text, this);
    m_bubble->show();
}

void HelpButton::hideBubble()
{
    if (m_bubble)
        m_bubble->close();
    m_bubble = nullptr;
}
