#include "FileTypes.h"

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
/// The MIME types, as data/<app id>.mime.xml installs them.
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

void registerWithDesktop(const QList<std::pair<FileTypes::Type, QImage>> &icons, const QString &themeName)
{
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    const QByteArray xml = mimeXml();
    if (!xml.isEmpty() && update(data.filePath(QStringLiteral("mime/packages/" APP_ID ".xml")), xml))
        QProcess::execute(QStringLiteral("update-mime-database"), {data.filePath(QStringLiteral("mime"))});

    // In hicolor, where every theme ends, and in the user's theme itself:
    // GTK 4 looks for every name of a type's icon in a theme before going
    // to the next one, so the generic document of the theme (or of one it
    // inherits, Adwaita) would win over ours, kept in hicolor only.
    QStringList themes{QStringLiteral("hicolor")};
    if (!themeName.isEmpty() && themeName != QLatin1String("hicolor"))
        themes << themeName;
    const QDir iconsDir(data.filePath(QStringLiteral("icons")));
    // The names of earlier versions go from every theme.
    for (const QString &name : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        for (const QString &size : QDir(iconsDir.filePath(name)).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            for (const char *old : {APP_ID "-project", APP_ID "-database"})
                QFile::remove(iconsDir.filePath(QStringLiteral("%1/%2/mimetypes/%3.png").arg(name, size, QLatin1String(old))));
    for (const QString &other : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (themes.contains(other))
            continue;
        // A theme used before: ours go from there.
        for (const auto &[type, icon] : icons) {
            for (const QString &file : QDir(iconsDir.filePath(other))
                                           .entryList({QStringLiteral("*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
                QFile::remove(iconsDir.filePath(QStringLiteral("%1/%2/mimetypes/%3.png").arg(other, file, type.iconName)));
            }
        }
    }
    for (const QString &name : themes) {
        // The sizes the theme has folders of mime type icons for.
        const QString index = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                     QStringLiteral("icons/%1/index.theme").arg(name));
        QFile indexFile(index);
        const QString listed = indexFile.open(QIODevice::ReadOnly) ? QString::fromUtf8(indexFile.readAll()) : QString();
        bool changed = false;
        for (const auto &[type, icon] : icons) {
            if (icon.isNull())
                continue;
            for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
                const QString folder = QStringLiteral("%1x%1/mimetypes").arg(size);
                if (name != QLatin1String("hicolor") && !listed.contains(QLatin1Char('[') + folder + QLatin1Char(']')))
                    continue;
                const QImage scaled = size == icon.width() ? icon : icon.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                changed |= update(iconsDir.filePath(QStringLiteral("%1/%2/%3.png").arg(name, folder, type.iconName)), pngOf(scaled));
            }
        }
        // A theme with a cache is looked up in the cache only.
        const QString root = iconsDir.filePath(name);
        if (changed && QFileInfo::exists(root + QStringLiteral("/icon-theme.cache")))
            QProcess::execute(QStringLiteral("gtk-update-icon-cache"), {QStringLiteral("-q"), QStringLiteral("-t"), QStringLiteral("-f"), root});
    }

    // Pragma Chess opens them, unless the user chose another application.
    const QString entry = QStringLiteral(APP_ID ".desktop");
    if (QStandardPaths::locate(QStandardPaths::ApplicationsLocation, entry).isEmpty())
        return;
    for (const auto &[type, icon] : icons) {
        QProcess query;
        query.start(QStringLiteral("xdg-mime"), {QStringLiteral("query"), QStringLiteral("default"), type.mimeType});
        if (query.waitForFinished(5000) && query.exitCode() == 0 && query.readAllStandardOutput().trimmed().isEmpty())
            QProcess::execute(QStringLiteral("xdg-mime"), {QStringLiteral("default"), entry, type.mimeType});
    }
}
#endif

#if defined(Q_OS_WIN)
/// The installer's ProgId, for this user, with the icon and this executable:
/// it serves a copy that was not installed too. The extension's default is
/// set only where there is none (.pdb is also Visual Studio's).
void registerWithShell(const FileTypes::Type &type, const QString &iconFile)
{
    const QString classes = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
                                                               QStringLiteral("%1"));
    QSettings progId(classes + QStringLiteral("\\") + type.progId, QSettings::NativeFormat);
    progId.setValue(QStringLiteral("Default"), type.description);
    progId.setValue(QStringLiteral("DefaultIcon/Default"), QDir::toNativeSeparators(iconFile));
    progId.setValue(QStringLiteral("shell/open/command/Default"), command);
    progId.sync();
    QSettings extension(classes + QStringLiteral("\\") + type.extension, QSettings::NativeFormat);
    extension.setValue(QStringLiteral("OpenWithProgids/") + type.progId, QString());
    if (extension.value(QStringLiteral("Default")).toString().isEmpty())
        extension.setValue(QStringLiteral("Default"), type.progId);
    extension.sync();
}
#endif

} // namespace

namespace FileTypes {

QList<Type> types()
{
    return {
        {QStringLiteral("application/x-pragma-chess-project"), QStringLiteral("io.github.francescobianco.pragma-chess-project"),
         QStringLiteral("PragmaChess.Project"), QStringLiteral(".pch"),
         QCoreApplication::translate("FileTypes", "Pragma Chess Project"), FolderIcon::pawnPath()},
        {QStringLiteral("application/x-pragma-chess-database"), QStringLiteral("io.github.francescobianco.pragma-chess-database"),
         QStringLiteral("PragmaChess.Database"), QStringLiteral(".pdb"),
         QCoreApplication::translate("FileTypes", "Pragma Chess Database"), FolderIcon::databasePath()},
    };
}

QImage compose(const QIcon &document, const QPainterPath &mark, int size)
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
    // The mark a little taller than the lines were, centred where they were.
    const qreal side = std::min(box.height() * 1.15, box.width());
    const QRectF place(box.center().x() - side / 2, box.center().y() - side / 2, side, side);
    painter.translate(place.topLeft());
    painter.scale(place.width(), place.height());
    painter.fillPath(mark, ink);
    return image;
}

void registerWithSystem()
{
#if defined(Q_OS_MACOS)
    // The bundle declares the types (Info.plist): nothing to do at run time.
#else
    const QIcon document = systemDocument();
    QList<std::pair<Type, QImage>> icons;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const Type &type : types()) {
        icons.append({type, compose(document, type.mark, 256)});
        hash.addData(pngOf(icons.constLast().second));
    }
    hash.addData(QCoreApplication::applicationFilePath().toUtf8());
#if !defined(Q_OS_WIN)
    hash.addData(mimeXml());
    const QString themeName = FolderIcon::iconThemeName();
    hash.addData(themeName.toUtf8());
#endif
    const QString stamp = QString::fromLatin1(hash.result().toHex().left(12));
    QSettings settings;
    if (settings.value(QStringLiteral("fileTypes/registered")).toString() == stamp)
        return;
#if defined(Q_OS_WIN)
    // A new name for every new picture: Explorer keeps icons by path.
    const QDir directory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    directory.mkpath(QStringLiteral("."));
    QStringList written;
    for (const auto &[type, icon] : icons) {
        if (icon.isNull())
            continue;
        const QString name = type.extension.mid(1) + QLatin1Char('-') + stamp + QStringLiteral(".ico");
        if (!FolderIcon::writeFile(directory.filePath(name), FolderIcon::icoOf(icon)))
            return;
        registerWithShell(type, directory.filePath(name));
        written << name;
    }
    for (const QString &old : directory.entryList({QStringLiteral("pch-*.ico"), QStringLiteral("pdb-*.ico"), QStringLiteral("project-*.ico")}, QDir::Files))
        if (!written.contains(old))
            QFile::remove(directory.filePath(old));
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#else
    registerWithDesktop(icons, themeName);
#endif
    settings.setValue(QStringLiteral("fileTypes/registered"), stamp);
#endif
}

} // namespace FileTypes
