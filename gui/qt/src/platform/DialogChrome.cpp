#include "DialogChrome.h"

#include <QDialog>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

namespace {

/// The transparent margin that holds the shadow, the height of the title
/// bar and the radius of the panel's corners.
constexpr int kShadow = 22;
constexpr int kTitleHeight = 38;
constexpr qreal kRadius = 10;
constexpr int kCloseSize = 24;
/// How near the panel's edge a press resizes the dialog.
constexpr int kResizeGrip = 6;

} // namespace

void DialogChrome::install(QDialog *dialog)
{
    if (dialog->property("_pragma_chrome").toBool())
        return;
    dialog->setProperty("_pragma_chrome", true);
    new DialogChrome(dialog);
}

DialogChrome::DialogChrome(QDialog *dialog)
    : QObject(dialog)
    , m_dialog(dialog)
{
    // No decoration from Qt: the frame is drawn here. A dialog is polished
    // in the middle of being shown, when its window already exists, and
    // setWindowFlags() would hide it again: tell the window itself.
    const Qt::WindowFlags flags = dialog->windowFlags() | Qt::FramelessWindowHint;
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    if (QWindow *window = dialog->windowHandle()) {
        dialog->overrideWindowFlags(flags);
        window->setFlags(flags);
        // The native window was made opaque and stays so whatever is asked
        // later: drop it, and showing the dialog makes it again, translucent.
        window->destroy();
    } else {
        dialog->setWindowFlags(flags);
    }
    dialog->setContentsMargins(kShadow, kShadow + kTitleHeight, kShadow, kShadow);
    // The sizes the dialog asked for were for its contents: the frame is extra.
    const QSize frame(2 * kShadow, 2 * kShadow + kTitleHeight);
    if (dialog->minimumWidth() > 0)
        dialog->setMinimumWidth(dialog->minimumWidth() + frame.width());
    if (dialog->minimumHeight() > 0)
        dialog->setMinimumHeight(dialog->minimumHeight() + frame.height());
    if (dialog->maximumWidth() < QWIDGETSIZE_MAX)
        dialog->setMaximumWidth(dialog->maximumWidth() + frame.width());
    if (dialog->maximumHeight() < QWIDGETSIZE_MAX)
        dialog->setMaximumHeight(dialog->maximumHeight() + frame.height());
    if (dialog->testAttribute(Qt::WA_Resized))
        dialog->resize(dialog->size() + frame);
    dialog->setMouseTracking(true);
    dialog->installEventFilter(this);
}

QRect DialogChrome::panelRect() const
{
    return m_dialog->rect().adjusted(kShadow, kShadow, -kShadow, -kShadow);
}

QRect DialogChrome::closeRect() const
{
    const QRect panel = panelRect();
    const int inset = (kTitleHeight - kCloseSize) / 2;
    return QRect(panel.right() - inset - kCloseSize + 1, panel.top() + inset, kCloseSize, kCloseSize);
}

bool DialogChrome::isResizable() const
{
    return m_dialog->minimumSize() != m_dialog->maximumSize();
}

Qt::Edges DialogChrome::edgesAt(const QPoint &position) const
{
    Qt::Edges edges;
    if (!isResizable())
        return edges;
    const QRect panel = panelRect();
    if (!panel.adjusted(-kResizeGrip, -kResizeGrip, kResizeGrip, kResizeGrip).contains(position))
        return edges;
    if (position.x() < panel.left() + kResizeGrip)
        edges |= Qt::LeftEdge;
    if (position.x() > panel.right() - kResizeGrip)
        edges |= Qt::RightEdge;
    if (position.y() < panel.top() + kResizeGrip)
        edges |= Qt::TopEdge;
    if (position.y() > panel.bottom() - kResizeGrip)
        edges |= Qt::BottomEdge;
    return edges;
}

bool DialogChrome::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_dialog)
        return false;
    switch (event->type()) {
    case QEvent::Paint:
        paint(); // Under the dialog's own painting and its children.
        return false;
    case QEvent::WindowTitleChange:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        m_dialog->update();
        return false;
    case QEvent::MouseButtonPress:
        return press(static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const bool chosen = m_closePressed && closeRect().contains(mouse->pos());
        m_closePressed = false;
        m_dialog->update(closeRect());
        if (chosen)
            m_dialog->close();
        return chosen;
    }
    case QEvent::MouseMove:
        hover(static_cast<QMouseEvent *>(event)->pos());
        return false;
    case QEvent::Leave:
        hover(QPoint(-1, -1));
        return false;
    default:
        return false;
    }
}

bool DialogChrome::press(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return false;
    QWindow *window = m_dialog->windowHandle();
    if (closeRect().contains(event->pos())) {
        m_closePressed = true;
        m_dialog->update(closeRect());
        return true;
    }
    if (const Qt::Edges edges = edgesAt(event->pos()); edges && window) {
        window->startSystemResize(edges);
        return true;
    }
    const QRect panel = panelRect();
    const QRect title(panel.left(), panel.top(), panel.width(), kTitleHeight);
    if (title.contains(event->pos()) && window) {
        window->startSystemMove();
        return true;
    }
    return false;
}

void DialogChrome::hover(const QPoint &position)
{
    const bool onClose = closeRect().contains(position);
    if (onClose != m_closeHovered) {
        m_closeHovered = onClose;
        m_dialog->update(closeRect());
    }
    const Qt::Edges edges = onClose ? Qt::Edges() : edgesAt(position);
    const bool horizontal = edges & (Qt::LeftEdge | Qt::RightEdge);
    const bool vertical = edges & (Qt::TopEdge | Qt::BottomEdge);
    const bool falling = edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge);
    if (horizontal && vertical)
        m_dialog->setCursor(falling ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
    else if (horizontal)
        m_dialog->setCursor(Qt::SizeHorCursor);
    else if (vertical)
        m_dialog->setCursor(Qt::SizeVerCursor);
    else
        m_dialog->unsetCursor();
}

void DialogChrome::paint()
{
    QPainter painter(m_dialog);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF panel = QRectF(panelRect());
    const QPalette palette = m_dialog->palette();

    // The shadow: darker near the panel, fading across the margin, and a
    // little lower than the panel, as light from above casts it. The window
    // in use casts a deeper one.
    const qreal depth = m_dialog->isActiveWindow() ? 1.0 : 0.6;
    painter.setPen(Qt::NoPen);
    for (int spread = kShadow; spread >= 1; --spread) {
        const qreal fade = 1.0 - qreal(spread) / kShadow;
        painter.setBrush(QColor(0, 0, 0, qRound(depth * (2.5 + 14 * fade * fade))));
        const QRectF ring = panel.adjusted(-spread, -spread + 4, spread, spread + 3).intersected(QRectF(m_dialog->rect()));
        painter.drawRoundedRect(ring, kRadius + spread, kRadius + spread);
    }

    // The panel, over whatever the shadow put under it.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    // On a dark theme a darker edge would vanish: there the edge is lighter.
    const QColor window = palette.color(QPalette::Window);
    painter.setBrush(window);
    painter.setPen(QPen(window.lightness() < 128 ? window.lighter(170) : window.darker(150), 1));
    painter.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // The title, in the middle, and the close button at its right.
    const QRect close = closeRect();
    QFont font = m_dialog->font();
    font.setBold(true);
    painter.setFont(font);
    const QPalette::ColorGroup group = m_dialog->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    QColor text = palette.color(group, QPalette::WindowText);
    if (!m_dialog->isActiveWindow())
        text.setAlphaF(0.6f);
    painter.setPen(text);
    const int side = kTitleHeight; // As much room left of the title as the button takes at its right.
    const QRect title(panelRect().left() + side, panelRect().top(), panelRect().width() - 2 * side, kTitleHeight);
    painter.drawText(title, Qt::AlignCenter,
                     painter.fontMetrics().elidedText(m_dialog->windowTitle(), Qt::ElideRight, title.width()));

    QColor disc = palette.color(QPalette::WindowText);
    disc.setAlphaF(m_closePressed ? 0.28f : m_closeHovered ? 0.18f : 0.1f);
    painter.setPen(Qt::NoPen);
    painter.setBrush(disc);
    painter.drawEllipse(close);
    painter.setPen(QPen(text, 1.6, Qt::SolidLine, Qt::RoundCap));
    const QPointF center = QRectF(close).center();
    const qreal arm = 4;
    painter.drawLine(center + QPointF(-arm, -arm), center + QPointF(arm, arm));
    painter.drawLine(center + QPointF(-arm, arm), center + QPointF(arm, -arm));
}
