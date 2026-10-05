#include "DialogFrame.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

void DialogFrame::install()
{
#ifdef Q_OS_WIN
    qApp->installEventFilter(new DialogFrame(qApp));
#endif
}

bool DialogFrame::eventFilter(QObject *watched, QEvent *event)
{
#ifdef Q_OS_WIN
    // At Show the native window exists and Qt has given it its icon already.
    if (event->type() == QEvent::Show) {
        auto *dialog = qobject_cast<QDialog *>(watched);
        if (dialog && dialog->isWindow()) {
            const HWND window = reinterpret_cast<HWND>(dialog->winId());
            // A modal dialog frame has no icon when none is set on the window
            // itself: the class icon (the application's) is not used.
            SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_DLGMODALFRAME);
            SendMessageW(window, WM_SETICON, ICON_SMALL, 0);
            SendMessageW(window, WM_SETICON, ICON_BIG, 0);
            SetWindowPos(window, nullptr, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
#endif
    return QObject::eventFilter(watched, event);
}
