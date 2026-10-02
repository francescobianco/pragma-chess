#pragma once

#include <QObject>

class QDialog;
class QMouseEvent;

/// The frame of a dialog where nobody else draws one worth having: on
/// GNOME's Wayland the compositor decorates nothing, and the decoration Qt
/// falls back to is a bare title bar with no shadow, so a dialog lies flat
/// on the window behind it. DialogChrome takes that decoration away and
/// draws the dialog as the desktop's own are: a rounded panel with a title
/// and a close button, lifted by a shadow that lives in a transparent margin
/// around it. The dialog's contents move inside by that margin; nothing else
/// about the dialog changes.
class DialogChrome : public QObject {
public:
    /// Dresses `dialog`; called when it is polished, before it is shown.
    static void install(QDialog *dialog);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit DialogChrome(QDialog *dialog);

    void paint();
    /// The panel inside the shadow, and its parts.
    QRect panelRect() const;
    QRect closeRect() const;
    /// The edges of the panel under `position`, for resizing; none inside it.
    Qt::Edges edgesAt(const QPoint &position) const;
    bool isResizable() const;
    bool press(QMouseEvent *event);
    void hover(const QPoint &position);

    QDialog *m_dialog;
    bool m_closeHovered = false;
    bool m_closePressed = false;
};
