#pragma once

#include <QProxyStyle>

/// Qt has no GTK widget style, so on GTK-based desktops (GNOME, Cinnamon,
/// MATE, XFCE, …) widgets are drawn by Fusion. This proxy removes the Fusion
/// details that visibly clash with GTK applications: bevels and rules around
/// the menu bar and toolbars, separator lines, dotted splitter grips and
/// always-visible mnemonic underlines. It also gives the menu bar and
/// drop-down menus GTK-like spacing.
///
/// On Wayland nobody draws a shadow under a popup: GTK applications draw
/// their own, and so does this style for menus, or they would lie flat on
/// the window and be hard to tell from it. The menu's window grows by a
/// transparent margin (the menu's frame width) that holds the shadow.
/// Dialogs and the main window get the same treatment from WindowChrome: a
/// frame of their own with a shadow, in place of the bare decoration Qt
/// falls back to.
class GtkDesktopStyle : public QProxyStyle {
public:
    GtkDesktopStyle();

    /// Whether the current session is a GTK-based desktop.
    static bool isGtkBasedDesktop();
    /// Whether menus and dialogs draw their own shadow: on by default on Wayland only.
    void setMenuShadows(bool enabled);

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
    void polish(QWidget *widget) override;
    using QProxyStyle::polish;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Whether menus and dialogs draw their own shadow (Wayland).
    bool m_menuShadows = false;
};
