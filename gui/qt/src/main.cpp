#include "MainWindow.h"
#include "app/UiLanguage.h"
#include "platform/DialogFrame.h"
#include "platform/GtkDesktopStyle.h"

#include <QApplication>
#include <QIcon>

#ifdef Q_OS_UNIX
#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int signalFds[2];

void onTerminationSignal(int)
{
    char byte = 1;
    [[maybe_unused]] ssize_t written = ::write(signalFds[0], &byte, sizeof(byte));
}

// Turns SIGTERM/SIGINT/SIGHUP into a clean quit, so the session is saved when
// the app is stopped from a terminal or by `make start`, without any dialog.
void installTerminationHandler(QApplication &app, MainWindow &window)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFds) != 0)
        return;
    auto *notifier = new QSocketNotifier(signalFds[1], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, &app, [notifier, &window] {
        notifier->setEnabled(false);
        char byte;
        [[maybe_unused]] ssize_t bytesRead = ::read(signalFds[1], &byte, sizeof(byte));
        // Qt 6 closes the windows on quit(), which would ask to save: a signal
        // is not the user closing the window, so close without asking.
        window.quitWithoutAsking();
        QApplication::quit();
    });

    struct sigaction action {};
    action.sa_handler = onTerminationSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    for (int signal : {SIGTERM, SIGINT, SIGHUP})
        sigaction(signal, &action, nullptr);
}

} // namespace
#endif

int main(int argc, char *argv[])
{
    // Menu entries are text only, as in GNOME and macOS; icons stay in toolbars.
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
    QApplication app(argc, argv);
    // Qt 6.4's Wayland plugin asks the compositor to activate the focus window
    // every time the focus object changes, so also for every menu that opens.
    // GNOME cannot grant it and marks the window as demanding attention, and
    // the Ubuntu Dock then slides in over the application. That request is
    // the only thing connected to this signal here: take it away.
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
        QObject::disconnect(&app, SIGNAL(focusObjectChanged(QObject*)), nullptr, nullptr);
    QApplication::setOrganizationName(QStringLiteral("Pragma"));
    QApplication::setApplicationName(QStringLiteral("pragma-chess"));
    // No application display name: Qt would append " — Pragma Chess" to the
    // title of every window, dialogs included. The main window writes the
    // application's name in its own title (MainWindow::updateWindowTitle).
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    UiLanguage::install(app);
    // Lets GNOME and KDE match windows to the installed .desktop entry.
    QGuiApplication::setDesktopFileName(QStringLiteral(APP_ID));
    // The installed theme icon when there is one, the embedded logo otherwise.
    QIcon appIcon;
    for (int size : {16, 22, 24, 32, 48, 64, 128, 256})
        appIcon.addFile(QStringLiteral(":/icons/hicolor/%1x%1/apps/" APP_ID ".png").arg(size), QSize(size, size));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(APP_ID), appIcon));

    // Respect an explicit user choice (-style / QT_STYLE_OVERRIDE).
    if (GtkDesktopStyle::isGtkBasedDesktop() && qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE")
        && !app.arguments().contains(QStringLiteral("-style")))
        QApplication::setStyle(new GtkDesktopStyle);

    // Only the main window carries the logo in its title bar (Windows).
    DialogFrame::install();

    MainWindow window;
#ifdef Q_OS_UNIX
    installTerminationHandler(app, window);
#endif
    window.show();
    // Project files passed on the command line (e.g. opened from the file manager).
    const QStringList arguments = app.arguments().mid(1);
    for (const QString &argument : arguments) {
        if (argument.endsWith(QLatin1String(".pch"), Qt::CaseInsensitive)) {
            window.openProjectFile(argument);
            break;
        }
    }
    return app.exec();
}
