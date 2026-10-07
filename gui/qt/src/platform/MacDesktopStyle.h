#pragma once

#include <QProxyStyle>

/// What Qt's macOS style gets wrong for this window. Its separators between
/// panels (docks and splitters) are one pixel wide, and so is the area that
/// takes the mouse: hard to grab, and the resize cursor barely shows. Here
/// they take a few pixels of the window's colour, as Fusion's on Linux. Its toolbar is painted with
/// a gradient and a line under it, its separators dotted: plain here, the
/// window's own colour, separators a thin line, as on the other systems.
class MacDesktopStyle : public QProxyStyle {
public:
    MacDesktopStyle();

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override;
};
