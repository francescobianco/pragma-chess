#include "GtkDesktopStyle.h"

#include <QByteArrayList>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleOption>

GtkDesktopStyle::GtkDesktopStyle()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
{
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
    default:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}
