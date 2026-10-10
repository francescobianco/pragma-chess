#include "ProjectFileType.h"

#include "FolderIcon.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <shlobj.h>
#elif !defined(Q_OS_MACOS)
#include <QProcess>
#endif

namespace {

int difference(QRgb a, QRgb b)
{
    return std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) + std::abs(qBlue(a) - qBlue(b));
}

/// The commonest colour of the opaque pixels in `area` for which `keep` holds.
template<typename Keep>
std::optional<QRgb> commonest(const QImage &image, const QRect &area, Keep keep)
{
    QHash<QRgb, int> counts;
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) == 255 && keep(pixel))
                ++counts[pixel];
        }
    if (counts.isEmpty())
        return std::nullopt;
    return std::max_element(counts.cbegin(), counts.cend()).key();
}

#if !defined(Q_OS_MACOS)
/// The system's document of plain text, the page the pawn goes on.
QIcon systemDocument()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // The shell gives icons by file: a text file of its own.
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("page.txt"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.close();
    return QIcon(QFileIconProvider().icon(QFileInfo(path)).pixmap(256, 256));
#else
    const QList<QIcon> icons = FolderIcon::themeIcons({QStringLiteral("text-x-generic"), QStringLiteral("text-plain")});
    return icons.constFirst().isNull() ? icons.constLast() : icons.constFirst();
#endif
}

QByteArray pngOf(const QImage &image)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
QString iconName()
{
    return QStringLiteral(APP_ID "-project");
}

/// The MIME type, as data/<app id>.mime.xml installs it.
QByteArray mimeXml()
{
    QFile file(QStringLiteral(":/mime/" APP_ID ".mime.xml"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// Writes `data` at `path` unless it is there already; true when it wrote.
bool update(const QString &path, const QByteArray &data)
{
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == data)
        return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    return FolderIcon::writeFile(path, data);
}

void registerWithDesktop(const QImage &icon)
{
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    const QByteArray xml = mimeXml();
    if (!xml.isEmpty() && update(data.filePath(QStringLiteral("mime/packages/" APP_ID ".xml")), xml))
        QProcess::execute(QStringLiteral("update-mime-database"), {data.filePath(QStringLiteral("mime"))});

    bool iconsChanged = false;
    if (!icon.isNull()) {
        for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
            const QImage scaled = size == icon.width() ? icon : icon.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            iconsChanged |= update(data.filePath(QStringLiteral("icons/hicolor/%1x%1/mimetypes/%2.png").arg(size).arg(iconName())),
                                   pngOf(scaled));
        }
    }
    // A theme with a cache is looked up in the cache only.
    const QString hicolor = data.filePath(QStringLiteral("icons/hicolor"));
    if (iconsChanged && QFileInfo::exists(hicolor + QStringLiteral("/icon-theme.cache")))
        QProcess::execute(QStringLiteral("gtk-update-icon-cache"), {QStringLiteral("-q"), QStringLiteral("-t"), QStringLiteral("-f"), hicolor});

    // Pragma Chess opens projects, unless the user chose another application.
    const QString entry = QStringLiteral(APP_ID ".desktop");
    if (QStandardPaths::locate(QStandardPaths::ApplicationsLocation, entry).isEmpty())
        return;
    QProcess query;
    query.start(QStringLiteral("xdg-mime"), {QStringLiteral("query"), QStringLiteral("default"), QLatin1String(ProjectFileType::mimeType)});
    if (query.waitForFinished(5000) && query.exitCode() == 0 && query.readAllStandardOutput().trimmed().isEmpty())
        QProcess::execute(QStringLiteral("xdg-mime"), {QStringLiteral("default"), entry, QLatin1String(ProjectFileType::mimeType)});
}
#endif

#if defined(Q_OS_WIN)
/// The installer's ProgId, for this user, with the icon and this executable:
/// it serves a copy that was not installed too.
void registerWithShell(const QString &iconFile)
{
    const QString classes = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
                                                               QStringLiteral("%1"));
    QSettings progId(classes + QStringLiteral("\\PragmaChess.Project"), QSettings::NativeFormat);
    progId.setValue(QStringLiteral("Default"), QCoreApplication::translate("ProjectFileType", "Pragma Chess Project"));
    progId.setValue(QStringLiteral("DefaultIcon/Default"), QDir::toNativeSeparators(iconFile));
    progId.setValue(QStringLiteral("shell/open/command/Default"), command);
    progId.sync();
    QSettings extension(classes + QStringLiteral("\\.pch"), QSettings::NativeFormat);
    extension.setValue(QStringLiteral("OpenWithProgids/PragmaChess.Project"), QString());
    if (extension.value(QStringLiteral("Default")).toString().isEmpty())
        extension.setValue(QStringLiteral("Default"), QStringLiteral("PragmaChess.Project"));
    extension.sync();
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
#endif

} // namespace

namespace ProjectFileType {

QImage compose(const QIcon &document, int size)
{
    QImage image = FolderIcon::imageOf(document, size);
    if (image.isNull())
        return {};
    // The page: where the icon is, and its commonest colour.
    QRect page;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) > 0)
                page |= QRect(x, y, 1, 1);
    if (page.isEmpty())
        return {};
    const auto paper = commonest(image, page, [](QRgb) { return true; });
    if (!paper)
        return {};
    // The lines are written inside the page, below the folded corner and
    // away from its edges: what differs there from the paper.
    const QRect inside(page.left() + page.width() * 15 / 100, page.top() + page.height() * 36 / 100,
                       page.width() * 70 / 100, page.height() * 54 / 100);
    const auto written = [&](QRgb pixel) { return difference(pixel, *paper) > 60; };
    QRect lines;
    for (int y = inside.top(); y <= inside.bottom(); ++y)
        for (int x = inside.left(); x <= inside.right(); ++x)
            if (qAlpha(image.pixel(x, y)) == 255 && written(image.pixel(x, y)))
                lines |= QRect(x, y, 1, 1);
    const QColor paperColor = QColor::fromRgba(*paper);
    QColor ink;
    QRectF box;
    if (lines.width() > page.width() / 5 && lines.height() > page.height() / 6) {
        ink = QColor::fromRgba(commonest(image, lines, written).value_or(*paper));
        box = QRectF(lines);
    } else {
        // A blank page: the middle of it, in its colour made darker (or lighter).
        ink = paperColor.lightness() > 128 ? paperColor.darker(160) : paperColor.lighter(170);
        box = QRectF(page.left() + page.width() * 0.25, page.top() + page.height() * 0.38, page.width() * 0.5,
                     page.height() * 0.45);
    }

    // The lines go: each column of the page is drawn again from the paper
    // just above them to the paper just below, so its shading stays.
    const QRect erased = box.toAlignedRect().adjusted(-3, -3, 3, 3).intersected(page.adjusted(1, 1, -1, -1));
    if (erased.top() > page.top() && erased.bottom() < page.bottom()) {
        for (int x = erased.left(); x <= erased.right(); ++x) {
            const QColor above = QColor::fromRgba(image.pixel(x, erased.top() - 1));
            const QColor below = QColor::fromRgba(image.pixel(x, erased.bottom() + 1));
            for (int y = erased.top(); y <= erased.bottom(); ++y) {
                const qreal t = qreal(y - erased.top() + 1) / (erased.height() + 1);
                const auto mix = [t](int a, int b) { return qRound(a + (b - a) * t); };
                image.setPixel(x, y, qRgba(mix(above.red(), below.red()), mix(above.green(), below.green()),
                                           mix(above.blue(), below.blue()), mix(above.alpha(), below.alpha())));
            }
        }
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    // The pawn a little taller than the lines were, centred where they were.
    const qreal side = std::min(box.height() * 1.15, box.width());
    const QRectF pawn(box.center().x() - side / 2, box.center().y() - side / 2, side, side);
    painter.translate(pawn.topLeft());
    painter.scale(pawn.width(), pawn.height());
    painter.fillPath(FolderIcon::pawnPath(), ink);
    return image;
}

void registerWithSystem()
{
#if defined(Q_OS_MACOS)
    // The bundle declares the type (Info.plist): nothing to do at run time.
#else
    const QImage icon = compose(systemDocument(), 256);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(pngOf(icon));
    hash.addData(QCoreApplication::applicationFilePath().toUtf8());
#if !defined(Q_OS_WIN)
    hash.addData(mimeXml());
#endif
    const QString stamp = QString::fromLatin1(hash.result().toHex().left(12));
    QSettings settings;
    if (settings.value(QStringLiteral("projectFiles/registered")).toString() == stamp)
        return;
#if defined(Q_OS_WIN)
    if (icon.isNull())
        return;
    // A new name for every new picture: Explorer keeps icons by path.
    const QDir directory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    directory.mkpath(QStringLiteral("."));
    const QString iconFile = directory.filePath(QStringLiteral("project-") + stamp + QStringLiteral(".ico"));
    if (!FolderIcon::writeFile(iconFile, FolderIcon::icoOf(icon)))
        return;
    for (const QString &old : directory.entryList({QStringLiteral("project-*.ico")}, QDir::Files))
        if (directory.filePath(old) != iconFile)
            QFile::remove(directory.filePath(old));
    registerWithShell(iconFile);
#else
    registerWithDesktop(icon);
#endif
    settings.setValue(QStringLiteral("projectFiles/registered"), stamp);
#endif
}

} // namespace ProjectFileType
