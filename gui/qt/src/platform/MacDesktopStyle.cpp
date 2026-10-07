#include "MacDesktopStyle.h"

#include <QPainter>
#include <QStyleFactory>
#include <QStyleOption>

namespace {

/// The width of a separator between panels, as Fusion has it on Linux.
constexpr int kSeparatorExtent = 6;

} // namespace

MacDesktopStyle::MacDesktopStyle()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("macos")))
{
}

int MacDesktopStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_DockWidgetSeparatorExtent:
    case PM_SplitterWidth:
        return kSeparatorExtent;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

void MacDesktopStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                                  const QWidget *widget) const
{
    switch (element) {
    case CE_Splitter: // The gap between the panels' frames is enough, as on Linux.
    case CE_ToolBar:
        painter->fillRect(option->rect, option->palette.window());
        return;
    default:
        QProxyStyle::drawControl(element, option, painter, widget);
    }
}

void MacDesktopStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                                    const QWidget *widget) const
{
    switch (element) {
    case PE_IndicatorDockWidgetResizeHandle:
    case PE_PanelToolBar:
        painter->fillRect(option->rect, option->palette.window());
        return;
    case PE_IndicatorToolBarSeparator: {
        // A thin plain line, not macOS's dotted one.
        const QRect area = option->rect.adjusted(0, 6, 0, -6);
        const QRect line = option->state & State_Horizontal ? QRect(area.center().x(), area.top(), 1, area.height())
                                                            : QRect(area.left(), area.center().y(), area.width(), 1);
        painter->fillRect(line, option->palette.mid());
        return;
    }
    default:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}
