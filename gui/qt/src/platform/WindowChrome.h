#pragma once

#include <QObject>

class QMouseEvent;
class QWidget;

/// The frame of a window where nobody else draws one worth having: on
/// GNOME's Wayland the compositor decorates nothing, and the decoration Qt
/// falls back to is a bare title bar with no shadow, so a dialog lies flat
/// on the window behind it. WindowChrome takes that decoration away and
/// draws the window as the desktop's own are: a rounded panel with a title
/// and its buttons, lifted by a shadow that lives in a transparent margin
/// around it. The window's contents move inside by that margin; nothing
/// else about the window changes. A dialog has a close button; the main
/// window has minimize, maximize and close, and when it is maximized the
/// margin and the rounding go, as they do for every window of the desktop.
class WindowChrome : public QObject {
public:
    /// Dresses `window`, a top-level dialog or main window; called when it is
    /// polished, before it is shown.
    static void install(QWidget *window);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Button { None, Minimize, Maximize, Close };

    explicit WindowChrome(QWidget *window);

    /// How far the panel is from the window's edge: the shadow's room, none when maximized.
    int margin() const;
    bool isMaximized() const;
    bool isFullScreen() const;
    void applyMargins();
    /// Takes the size grip off the main window's status bar: the frame resizes.
    void dropSizeGrip();
    void paint();
    /// The panel inside the shadow, and its parts.
    QRect panelRect() const;
    QRect titleRect() const;
    QRect buttonRect(Button button) const;
    Button buttonAt(const QPoint &position) const;
    /// The edges of the panel under `position`, for resizing; none inside it.
    Qt::Edges edgesAt(const QPoint &position) const;
    bool isResizable() const;
    bool press(QMouseEvent *event);
    bool release(QMouseEvent *event);
    void hover(const QPoint &position);
    void toggleMaximized();

    QWidget *m_window;
    /// The window has minimize and maximize besides close.
    bool m_mainWindow;
    Button m_hovered = Button::None;
    Button m_pressed = Button::None;
};
