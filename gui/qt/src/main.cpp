#include "MainWindow.h"
#include "app/UiLanguage.h"
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

// Turns SIGTERM/SIGINT/SIGHUP into a regular QApplication::quit(), so the
// session is saved when the app is stopped from a terminal or by `make start`.
void installTerminationHandler(QApplication &app)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFds) != 0)
        return;
    auto *notifier = new QSocketNotifier(signalFds[1], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, &app, [notifier] {
        notifier->setEnabled(false);
        char byte;
        [[maybe_unused]] ssize_t bytesRead = ::read(signalFds[1], &byte, sizeof(byte));
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
    QApplication::setOrganizationName(QStringLiteral("Pragma"));
    QApplication::setApplicationName(QStringLiteral("pragma-chess"));
    QApplication::setApplicationDisplayName(QStringLiteral("Pragma Chess"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    UiLanguage::install(app);
    // Lets GNOME and KDE match windows to the installed .desktop entry.
    QGuiApplication::setDesktopFileName(QStringLiteral(APP_ID));
    // The installed theme icon when there is one, the embedded logo otherwise.
    QIcon appIcon;
    for (int size : {16, 22, 24, 32, 48, 64, 128, 256})
        appIcon.addFile(QStringLiteral(":/icons/hicolor/%1x%1/apps/" APP_ID ".png").arg(size), QSize(size, size));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(APP_ID), appIcon));

#ifdef Q_OS_UNIX
    installTerminationHandler(app);
#endif
    // Respect an explicit user choice (-style / QT_STYLE_OVERRIDE).
    if (GtkDesktopStyle::isGtkBasedDesktop() && qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE")
        && !app.arguments().contains(QStringLiteral("-style")))
        QApplication::setStyle(new GtkDesktopStyle);

    MainWindow window;
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
