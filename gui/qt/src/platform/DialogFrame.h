#pragma once

#include <QObject>

/// Dialogs without the application's icon in their title bar, as on GNOME
/// (WindowChrome draws them with none) and as Windows' own dialogs are:
/// only the main window carries the logo. Does something on Windows only,
/// where Qt puts the icon on every window.
class DialogFrame : public QObject {
public:
    /// Watches every dialog the application shows from now on.
    static void install();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    using QObject::QObject;
};
