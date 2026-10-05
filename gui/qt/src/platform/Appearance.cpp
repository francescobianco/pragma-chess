#include "Appearance.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

namespace {

/// What the application looks like now: until told otherwise, the system's.
AppearanceMode s_current = AppearanceMode::System;

/// Adwaita's colours, so the forced look is at home on GNOME.
QPalette adwaitaPalette(bool dark)
{
    const QColor window = dark ? QColor(0x35, 0x35, 0x35) : QColor(0xf6, 0xf5, 0xf4);
    const QColor base = dark ? QColor(0x2d, 0x2d, 0x2d) : QColor(0xff, 0xff, 0xff);
    const QColor alternate = dark ? QColor(0x32, 0x32, 0x32) : QColor(0xf6, 0xf5, 0xf4);
    const QColor text = dark ? QColor(0xee, 0xee, 0xec) : QColor(0x2e, 0x34, 0x36);
    const QColor button = dark ? QColor(0x3a, 0x3a, 0x3a) : QColor(0xed, 0xeb, 0xe9);
    const QColor highlight(0x35, 0x84, 0xe4);
    QColor disabledText = text;
    disabledText.setAlphaF(0.5);

    QPalette palette(button, window);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, dark ? QColor(0x1e, 0x1e, 0x1e) : QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, disabledText);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, dark ? QColor(0x78, 0xae, 0xed) : QColor(0x1b, 0x6a, 0xcb));
    palette.setColor(QPalette::LinkVisited, dark ? QColor(0xc0, 0x61, 0xcb) : QColor(0x81, 0x3d, 0x9c));
    palette.setColor(QPalette::Light, button.lighter(dark ? 130 : 110));
    palette.setColor(QPalette::Midlight, button.lighter(dark ? 115 : 105));
    palette.setColor(QPalette::Mid, button.darker(dark ? 120 : 130));
    palette.setColor(QPalette::Dark, button.darker(dark ? 150 : 160));
    palette.setColor(QPalette::Shadow, Qt::black);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, dark ? QColor(0x4a, 0x4a, 0x4a) : QColor(0xc0, 0xbf, 0xbc));
    return palette;
}

} // namespace

namespace Appearance {

void apply(AppearanceMode mode)
{
    if (mode == s_current)
        return;
    s_current = mode;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const Qt::ColorScheme scheme = mode == AppearanceMode::Light ? Qt::ColorScheme::Light
                                   : mode == AppearanceMode::Dark ? Qt::ColorScheme::Dark
                                                        : Qt::ColorScheme::Unknown;
    QGuiApplication::styleHints()->setColorScheme(scheme);
    if (mode == AppearanceMode::System || QGuiApplication::styleHints()->colorScheme() == scheme) {
        // The platform's own look: no palette of ours over it.
        QApplication::setPalette(QPalette());
        return;
    }
#endif
    // A palette without anything set gives the platform's back.
    QApplication::setPalette(mode == AppearanceMode::System ? QPalette() : adwaitaPalette(mode == AppearanceMode::Dark));
}

} // namespace Appearance
