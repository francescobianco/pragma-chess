#include "GtkDesktopStyle.h"

#include "WindowChrome.h"

#include <QAction>
#include <QByteArrayList>
#include <QDialog>
#include <QMainWindow>
#include <QEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>
#include <QStyleOption>

namespace {

/// The transparent margin around a menu that holds its shadow, and the
/// radius of the menu's corners.
constexpr int kMenuShadow = 12;
constexpr qreal kMenuRadius = 6;

/// A menu opened from an item of another menu that is on screen.
bool isSubMenu(const QMenu *menu)
{
    const QList<QObject *> owners = menu->menuAction()->associatedObjects();
    for (const QObject *owner : owners) {
        const auto *parent = qobject_cast<const QMenu *>(owner);
        if (parent && parent != menu && parent->isVisible())
            return true;
    }
    return false;
}

} // namespace

GtkDesktopStyle::GtkDesktopStyle()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    // X11 window managers shadow menus themselves, and may not composite at all.
    , m_menuShadows(QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
{
}

void GtkDesktopStyle::setMenuShadows(bool enabled)
{
    m_menuShadows = enabled;
}

void GtkDesktopStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (m_menuShadows && qobject_cast<QMenu *>(widget)) {
        widget->setAttribute(Qt::WA_TranslucentBackground);
        widget->installEventFilter(this);
    }
    // Dialogs and the main window would lie flat too: they get a frame with a shadow.
    if (m_menuShadows && widget->isWindow() && (qobject_cast<QDialog *>(widget) || qobject_cast<QMainWindow *>(widget)))
        WindowChrome::install(widget);
}

bool GtkDesktopStyle::eventFilter(QObject *watched, QEvent *event)
{
    // A menu opens with its corner where it was asked to: the margin of the
    // shadow goes outside. (A submenu is placed by PM_SubMenuOverlap.) The
    // show event comes before the window is shown, so it can still be moved.
    if (event->type() == QEvent::Show && m_menuShadows) {
        if (auto *menu = qobject_cast<QMenu *>(watched); menu && !isSubMenu(menu))
            menu->move(menu->pos() - QPoint(kMenuShadow, kMenuShadow));
    }
    return QProxyStyle::eventFilter(watched, event);
}

bool GtkDesktopStyle::isGtkBasedDesktop()
{
    static const QByteArrayList gtkDesktops{"GNOME", "UNITY", "X-CINNAMON", "MATE", "XFCE", "LXDE", "BUDGIE"};
    const QByteArrayList current = qgetenv("XDG_CURRENT_DESKTOP").toUpper().split(':');
    for (const QByteArray &desktop : current) {
        if (gtkDesktops.contains(desktop))
            return true;
    }
    return false;
}

int GtkDesktopStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                               QStyleHintReturn *returnData) const
{
    // GTK never underlines menu mnemonics (they still work with Alt).
    if (hint == SH_UnderlineShortcut)
        return 0;
    // GTK dialog buttons are text only.
    if (hint == SH_DialogButtonBox_ButtonsHaveIcons)
        return 0;
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

QSize GtkDesktopStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                        const QSize &size, const QWidget *widget) const
{
    QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
    if (type == CT_MenuBarItem && !result.isEmpty()) {
        // Fusion packs menu bar items tightly; GTK menu bars are more generous.
        result += QSize(8, 10);
    } else if (type == CT_MenuItem) {
        // Same for drop-down menu items (separators keep their height).
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (item && item->menuItemType != QStyleOptionMenuItem::Separator)
            result += QSize(24, 10);
    }
    return result;
}

int GtkDesktopStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                                 const QWidget *widget) const
{
    // Breathing room between the popup edge and its first/last item.
    if (metric == PM_MenuVMargin)
        return QProxyStyle::pixelMetric(metric, option, widget) + 4;
    if (m_menuShadows && qobject_cast<const QMenu *>(widget)) {
        // The frame of a menu is the margin its shadow is drawn in.
        if (metric == PM_MenuPanelWidth)
            return QProxyStyle::pixelMetric(metric, option, widget) + kMenuShadow;
        // A submenu opens against its parent's item: its own margin goes under the parent.
        if (metric == PM_SubMenuOverlap)
            return QProxyStyle::pixelMetric(metric, option, widget) - kMenuShadow;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

void GtkDesktopStyle::drawControl(ControlElement element, const QStyleOption *option,
                                  QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case CE_ToolBar:
        // Flat, like a GTK toolbar: no light/dark bevel lines.
        painter->fillRect(option->rect, option->palette.window());
        return;
    case CE_MenuBarEmptyArea:
    case CE_MenuBarItem: {
        // Fusion rules off the bottom of a main window menu bar; paint the
        // item without its last row so the bar blends into the toolbar.
        painter->save();
        painter->fillRect(option->rect, option->palette.window());
        painter->setClipRect(option->rect.adjusted(0, 0, 0, -1), Qt::IntersectClip);
        QProxyStyle::drawControl(element, option, painter, widget);
        painter->restore();
        return;
    }
    case CE_Splitter:
        // No dotted grip; the gap between panes is enough, as in GTK paned widgets.
        painter->fillRect(option->rect, option->palette.window());
        return;
    default:
        QProxyStyle::drawControl(element, option, painter, widget);
    }
}

void GtkDesktopStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                    QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case PE_IndicatorToolBarSeparator:
    case PE_IndicatorToolBarHandle:
        // GTK groups toolbar buttons with spacing, not with lines.
        return;
    case PE_IndicatorDockWidgetResizeHandle:
        painter->fillRect(option->rect, option->palette.window());
        return;
    case PE_FrameMenu:
        if (m_menuShadows && qobject_cast<const QMenu *>(widget))
            return; // Drawn with the panel.
        break;
    case PE_PanelMenu: {
        if (!m_menuShadows || !qobject_cast<const QMenu *>(widget))
            break;
        // The menu itself, inside the margin: rounded, with Fusion's colours.
        const QRectF panel = QRectF(option->rect).adjusted(kMenuShadow, kMenuShadow, -kMenuShadow, -kMenuShadow);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        // The shadow: darker near the menu, fading out across the margin and
        // a little lower than the menu, as light from above casts it.
        painter->setPen(Qt::NoPen);
        for (int spread = kMenuShadow; spread >= 1; --spread) {
            const qreal fade = 1.0 - qreal(spread) / kMenuShadow;
            painter->setBrush(QColor(0, 0, 0, qRound(2.5 + 12 * fade * fade)));
            const QRectF ring = panel.adjusted(-spread, -spread + 2, spread, spread + 1)
                                    .intersected(QRectF(option->rect));
            painter->drawRoundedRect(ring, kMenuRadius + spread, kMenuRadius + spread);
        }
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        // Fusion's colours; on a dark theme a darker edge would vanish, so it is lighter there.
        const QColor window = option->palette.window().color();
        painter->setBrush(option->palette.base().color().lighter(108));
        painter->setPen(QPen(window.lightness() < 128 ? window.lighter(170) : window.darker(160), 1));
        painter->drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), kMenuRadius, kMenuRadius);
        painter->restore();
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}
