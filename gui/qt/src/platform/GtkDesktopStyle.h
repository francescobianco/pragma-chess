#pragma once

#include <QProxyStyle>

/// Qt has no GTK widget style, so on GTK-based desktops (GNOME, Cinnamon,
/// MATE, XFCE, …) widgets are drawn by Fusion. This proxy removes the Fusion
/// details that visibly clash with GTK applications: bevels and rules around
/// the menu bar and toolbars, separator lines, dotted splitter grips and
/// always-visible mnemonic underlines. It also gives the menu bar and
/// drop-down menus GTK-like spacing.
class GtkDesktopStyle : public QProxyStyle {
public:
    GtkDesktopStyle();

    /// Whether the current session is a GTK-based desktop.
    static bool isGtkBasedDesktop();

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *returnData) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override;
};
