#include "WindowChrome.h"

#include <QDialog>
#include <QEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QPainterPath>
#include <QStatusBar>
#include <QWindow>

namespace {

/// The transparent margin that holds the shadow, the height of the title
/// bar and the radius of the panel's corners.
constexpr int kShadow = 22;
constexpr int kTitleHeight = 38;
constexpr qreal kRadius = 10;
constexpr int kButtonSize = 24;
constexpr int kButtonGap = 8;
/// How near the panel's edge a press resizes the window.
constexpr int kResizeGrip = 6;
/// Set on a window whose size already includes the frame (a restored geometry).
constexpr const char *kFramedProperty = "pragmaChromeFramed";

} // namespace

void WindowChrome::markFramed(QWidget *window)
{
    window->setProperty(kFramedProperty, true);
}

void WindowChrome::install(QWidget *window)
{
    if (window->property("_pragma_chrome").toBool())
        return;
    window->setProperty("_pragma_chrome", true);
    new WindowChrome(window);
}

WindowChrome::WindowChrome(QWidget *window)
    : QObject(window)
    , m_window(window)
    , m_mainWindow(qobject_cast<QMainWindow *>(window) != nullptr)
{
    // No decoration from Qt: the frame is drawn here. A dialog is polished
    // in the middle of being shown, when its window already exists, and
    // setWindowFlags() would hide it again: tell the window itself.
    const Qt::WindowFlags flags = window->windowFlags() | Qt::FramelessWindowHint;
    window->setAttribute(Qt::WA_TranslucentBackground);
    if (QWindow *native = window->windowHandle()) {
        window->overrideWindowFlags(flags);
        native->setFlags(flags);
        // The native window was made opaque and stays so whatever is asked
        // later: drop it, and showing the window makes it again, translucent.
        native->destroy();
    } else {
        window->setWindowFlags(flags);
    }
    applyMargins();
    // The sizes the window asked for were for its contents: the frame is extra.
    const QSize frame(2 * kShadow, 2 * kShadow + kTitleHeight);
    if (window->minimumWidth() > 0)
        window->setMinimumWidth(window->minimumWidth() + frame.width());
    if (window->minimumHeight() > 0)
        window->setMinimumHeight(window->minimumHeight() + frame.height());
    if (window->maximumWidth() < QWIDGETSIZE_MAX)
        window->setMaximumWidth(window->maximumWidth() + frame.width());
    if (window->maximumHeight() < QWIDGETSIZE_MAX)
        window->setMaximumHeight(window->maximumHeight() + frame.height());
    // A geometry restored from a previous run already holds the frame
    // (kFramedProperty): growing it again would make the window bigger at
    // every start, until it ran off the screen. Whatever the size, it never
    // starts larger than the screen it is on.
    if (window->testAttribute(Qt::WA_Resized) && !window->property(kFramedProperty).toBool())
        window->resize(window->size() + frame);
    if (const QScreen *screen = window->screen()) {
        const QSize room = screen->availableGeometry().size();
        if (window->width() > room.width() || window->height() > room.height())
            window->resize(window->size().boundedTo(room));
    }
    window->setMouseTracking(true);
    window->installEventFilter(this);
}

bool WindowChrome::isMaximized() const
{
    return m_window->windowState().testFlag(Qt::WindowMaximized);
}

bool WindowChrome::isFullScreen() const
{
    return m_window->windowState().testFlag(Qt::WindowFullScreen);
}

int WindowChrome::margin() const
{
    return isMaximized() || isFullScreen() ? 0 : kShadow;
}

void WindowChrome::applyMargins()
{
    // Full screen is the contents and nothing else. Otherwise the contents
    // keep off the panel's one-pixel edge, which they would paint over.
    const int side = margin();
    const int edge = side > 0 ? 1 : 0;
    m_window->setContentsMargins(side + edge, side + (isFullScreen() ? 0 : kTitleHeight), side + edge, side + edge);
}

void WindowChrome::dropSizeGrip()
{
    // The edges of the frame resize the window: the status bar's grip would
    // only sit, out of place, inside the bar's own margins.
    if (auto *mainWindow = qobject_cast<QMainWindow *>(m_window)) {
        if (auto *bar = mainWindow->findChild<QStatusBar *>())
            bar->setSizeGripEnabled(false);
    }
}

QRect WindowChrome::panelRect() const
{
    const int side = margin();
    return m_window->rect().adjusted(side, side, -side, -side);
}

QRect WindowChrome::titleRect() const
{
    const QRect panel = panelRect();
    return QRect(panel.left(), panel.top(), panel.width(), kTitleHeight);
}

QRect WindowChrome::buttonRect(Button button) const
{
    // From the right: close, then maximize, then minimize.
    int order = -1;
    switch (button) {
    case Button::Close: order = 0; break;
    case Button::Maximize: order = m_mainWindow ? 1 : -1; break;
    case Button::Minimize: order = m_mainWindow ? 2 : -1; break;
    case Button::None: break;
    }
    if (order < 0)
        return {};
    const QRect panel = panelRect();
    const int inset = (kTitleHeight - kButtonSize) / 2;
    const int right = panel.right() - inset - order * (kButtonSize + kButtonGap);
    return QRect(right - kButtonSize + 1, panel.top() + inset, kButtonSize, kButtonSize);
}

WindowChrome::Button WindowChrome::buttonAt(const QPoint &position) const
{
    for (const Button button : {Button::Close, Button::Maximize, Button::Minimize}) {
        if (buttonRect(button).contains(position))
            return button;
    }
    return Button::None;
}

bool WindowChrome::isResizable() const
{
    return !isMaximized() && !isFullScreen() && m_window->minimumSize() != m_window->maximumSize();
}

Qt::Edges WindowChrome::edgesAt(const QPoint &position) const
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

bool WindowChrome::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window)
        return false;
    switch (event->type()) {
    case QEvent::Paint:
        if (!isFullScreen())
            paint(); // Under the window's own painting and its children.
        return false;
    case QEvent::WindowStateChange:
        applyMargins();
        m_window->update();
        return false;
    case QEvent::Show:
    case QEvent::ChildAdded:
        // The status bar may come after the frame: catch it when it is there.
        QMetaObject::invokeMethod(this, [this] { dropSizeGrip(); }, Qt::QueuedConnection);
        return false;
    case QEvent::WindowTitleChange:
    case QEvent::ModifiedChange: // The "[*]" asterisk: a title change without the event.
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        m_window->update();
        return false;
    case QEvent::MouseButtonPress:
        return press(static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonDblClick: {
        // Double-clicking the title bar maximizes and restores, as on the desktop.
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (m_mainWindow && mouse->button() == Qt::LeftButton && titleRect().contains(mouse->pos())
            && buttonAt(mouse->pos()) == Button::None) {
            toggleMaximized();
            return true;
        }
        return false;
    }
    case QEvent::MouseButtonRelease:
        return release(static_cast<QMouseEvent *>(event));
    case QEvent::MouseMove:
        hover(static_cast<QMouseEvent *>(event)->pos());
        drag(static_cast<QMouseEvent *>(event));
        return false;
    case QEvent::Leave:
        hover(QPoint(-1, -1));
        return false;
    default:
        return false;
    }
}

bool WindowChrome::press(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || isFullScreen())
        return false;
    QWindow *native = m_window->windowHandle();
    if (const Button button = buttonAt(event->pos()); button != Button::None) {
        m_pressed = button;
        m_window->update(buttonRect(button));
        return true;
    }
    // A move or a resize starts only once the pointer has travelled: the
    // compositor takes the pointer over from the first step and Qt never sees
    // the release, so a plain click or a double click on the title bar must
    // not hand it over, or the clicks that follow are lost.
    if (const Qt::Edges edges = edgesAt(event->pos()); edges && native) {
        m_dragStart = event->pos();
        m_dragEdges = edges;
        return true;
    }
    if (titleRect().contains(event->pos()) && native) {
        m_dragStart = event->pos();
        m_dragEdges = Qt::Edges();
        return true;
    }
    return false;
}

void WindowChrome::drag(QMouseEvent *event)
{
    if (!m_dragStart || !(event->buttons() & Qt::LeftButton))
        return;
    if ((event->pos() - *m_dragStart).manhattanLength() < QApplication::startDragDistance())
        return;
    const Qt::Edges edges = m_dragEdges;
    m_dragStart.reset();
    if (QWindow *native = m_window->windowHandle()) {
        if (edges)
            native->startSystemResize(edges);
        else
            native->startSystemMove();
    }
}

bool WindowChrome::release(QMouseEvent *event)
{
    const bool wasDragging = m_dragStart.has_value();
    m_dragStart.reset();
    const Button pressed = std::exchange(m_pressed, Button::None);
    if (pressed == Button::None)
        return wasDragging; // A click on the frame that went nowhere: ours all the same.
    if (pressed == Button::None)
        return false;
    m_window->update(buttonRect(pressed));
    if (buttonAt(event->pos()) != pressed)
        return true; // Let go elsewhere: nothing happens.
    switch (pressed) {
    case Button::Close: m_window->close(); break;
    case Button::Minimize: m_window->showMinimized(); break;
    case Button::Maximize: toggleMaximized(); break;
    case Button::None: break;
    }
    return true;
}

void WindowChrome::toggleMaximized()
{
    if (isMaximized())
        m_window->showNormal();
    else
        m_window->showMaximized();
}

void WindowChrome::hover(const QPoint &position)
{
    const Button onButton = buttonAt(position);
    if (onButton != m_hovered) {
        m_hovered = onButton;
        m_window->update(titleRect());
    }
    const Qt::Edges edges = onButton != Button::None ? Qt::Edges() : edgesAt(position);
    const bool horizontal = edges & (Qt::LeftEdge | Qt::RightEdge);
    const bool vertical = edges & (Qt::TopEdge | Qt::BottomEdge);
    const bool falling = edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge);
    if (horizontal && vertical)
        m_window->setCursor(falling ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
    else if (horizontal)
        m_window->setCursor(Qt::SizeHorCursor);
    else if (vertical)
        m_window->setCursor(Qt::SizeVerCursor);
    else
        m_window->unsetCursor();
}

void WindowChrome::paint()
{
    QPainter painter(m_window);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF panel = QRectF(panelRect());
    const QPalette palette = m_window->palette();
    const bool active = m_window->isActiveWindow();
    // Maximized, the window fills the screen: no shadow, no rounding. The
    // main window's bottom corners are square anyway: its status bar reaches
    // them, and a dialog's buttons do not.
    const qreal radius = isMaximized() ? 0 : kRadius;
    const qreal bottomRadius = m_mainWindow ? 0 : radius;

    if (margin() > 0) {
        // The shadow: darker near the panel, fading across the margin, and a
        // little lower than the panel, as light from above casts it. The
        // window in use casts a deeper one.
        const qreal depth = active ? 1.0 : 0.6;
        painter.setPen(Qt::NoPen);
        for (int spread = kShadow; spread >= 1; --spread) {
            const qreal fade = 1.0 - qreal(spread) / kShadow;
            painter.setBrush(QColor(0, 0, 0, qRound(depth * (1.5 + 9 * fade * fade))));
            const QRectF ring = panel.adjusted(-spread, -spread + 4, spread, spread + 3)
                                    .intersected(QRectF(m_window->rect()));
            painter.drawRoundedRect(ring, radius + spread, radius + spread);
        }
    }

    // The panel, over whatever the shadow put under it. On a dark theme a
    // darker edge would vanish: there the edge is lighter.
    QPainterPath shape;
    const QRectF inner = panel.adjusted(0.5, 0.5, -0.5, -0.5);
    // Clockwise from the left end of the top edge; a square corner is a
    // point, since arcTo() does nothing with an empty rectangle.
    const auto corner = [&shape](const QPointF &point, qreal r, qreal startAngle, const QPointF &towards) {
        if (r > 0)
            shape.arcTo(QRectF(point.x() - r + r * towards.x(), point.y() - r + r * towards.y(), 2 * r, 2 * r), startAngle, -90);
        else
            shape.lineTo(point);
    };
    shape.moveTo(inner.left(), inner.top() + radius);
    corner(inner.topLeft(), radius, 180, QPointF(1, 1));
    corner(inner.topRight(), radius, 90, QPointF(-1, 1));
    corner(inner.bottomRight(), bottomRadius, 0, QPointF(-1, -1));
    corner(inner.bottomLeft(), bottomRadius, 270, QPointF(1, -1));
    shape.closeSubpath();
    const QColor window = palette.color(QPalette::Window);
    // The panel replaces the shadow under it (Source), opaque. The edge is
    // stroked afterwards, blended over that opaque fill: stroked together
    // with the fill in Source mode, its antialiased pixels replaced the
    // panel with half-transparent ones, and the edge let the desktop show
    // through — brighter over a white window, cut where a dark panel sat
    // inside. No edge when the window fills the screen, as on the desktop.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setPen(Qt::NoPen);
    painter.setBrush(window);
    if (margin() > 0) {
        painter.drawPath(shape);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(window.lightness() < 128 ? window.lighter(170) : window.darker(150), 1));
        painter.drawPath(shape);
    } else {
        // Maximized or full screen: the whole window, to the pixel. The path
        // runs half a pixel inside, and antialiased it left the outermost
        // row and column half transparent — a thin white line over a white
        // window behind, cut by the panels that reach the edge.
        painter.fillRect(m_window->rect(), window);
    }
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // The title, in the middle, and the buttons at its right.
    QFont font = m_window->font();
    font.setBold(true);
    painter.setFont(font);
    QColor text = palette.color(active ? QPalette::Active : QPalette::Inactive, QPalette::WindowText);
    if (!active)
        text.setAlphaF(0.6f);
    painter.setPen(text);
    const QRect title = titleRect();
    const int buttons = m_mainWindow ? 3 : 1;
    const int side = (kTitleHeight - kButtonSize) / 2 + buttons * (kButtonSize + kButtonGap); // Room for the buttons, both sides.
    const QRect caption(title.left() + side, title.top(), title.width() - 2 * side, title.height());
    // The native window's title has the "[*]" placeholder resolved.
    const QString shown = m_window->windowHandle() ? m_window->windowHandle()->title() : m_window->windowTitle();
    painter.drawText(caption, Qt::AlignCenter, painter.fontMetrics().elidedText(shown, Qt::ElideRight, caption.width()));
    if (m_mainWindow) {
        // The application's logo in the left corner, where desktops put the window's icon.
        const QIcon icon = m_window->windowIcon().isNull() ? QApplication::windowIcon() : m_window->windowIcon();
        const int inset = (kTitleHeight - kButtonSize) / 2;
        const QRect logo(title.left() + inset, title.top() + inset, kButtonSize, kButtonSize);
        icon.paint(&painter, logo, Qt::AlignCenter, active ? QIcon::Normal : QIcon::Disabled);
    }

    for (const Button button : {Button::Minimize, Button::Maximize, Button::Close}) {
        const QRect rect = buttonRect(button);
        if (rect.isNull())
            continue;
        QColor disc = palette.color(QPalette::WindowText);
        disc.setAlphaF(m_pressed == button ? 0.28f : m_hovered == button ? 0.18f : 0.1f);
        painter.setPen(Qt::NoPen);
        painter.setBrush(disc);
        painter.drawEllipse(rect);
        painter.setPen(QPen(text, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        const QPointF center = QRectF(rect).center();
        const qreal arm = 4;
        switch (button) {
        case Button::Close:
            painter.drawLine(center + QPointF(-arm, -arm), center + QPointF(arm, arm));
            painter.drawLine(center + QPointF(-arm, arm), center + QPointF(arm, -arm));
            break;
        case Button::Minimize:
            painter.drawLine(center + QPointF(-arm, 3), center + QPointF(arm, 3));
            break;
        case Button::Maximize:
            if (isMaximized()) // Restore: two windows, one behind the other.
                painter.drawRect(QRectF(center.x() - arm, center.y() - arm + 2.5, 2 * arm - 2.5, 2 * arm - 2.5));
            else
                painter.drawRect(QRectF(center.x() - arm, center.y() - arm, 2 * arm, 2 * arm));
            break;
        case Button::None:
            break;
        }
    }
}
