#include "FolderIcon.h"

#include "app/UserFolders.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <optional>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <shlobj.h>
#elif defined(Q_OS_MACOS)
#include <objc/message.h>
#include <objc/runtime.h>
#else
#include <QProcess>
#include <QUrl>
#endif

namespace {

/// A pixel of `example` far enough from the plain folder's to be its emblem
/// (not the antialiasing of an edge drawn a hair differently).
constexpr int kEmblemDifference = 40;

int difference(QRgb a, QRgb b)
{
    return std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) + std::abs(qBlue(a) - qBlue(b))
        + std::abs(qAlpha(a) - qAlpha(b));
}

/// Where a folder of the theme carries its emblem, and in what colour.
struct Emblem {
    QRectF box;
    QColor color;
};

/// The emblem as `examples` draw it on `folder`: the average of their
/// boxes, made square around their centre, and their commonest colour.
std::optional<Emblem> emblemOf(const QImage &folder, const QList<QImage> &examples)
{
    QPointF centre;
    qreal side = 0;
    QHash<QRgb, int> colours;
    int found = 0;
    for (const QImage &example : examples) {
        if (example.size() != folder.size())
            continue;
        int left = folder.width(), top = folder.height(), right = -1, bottom = -1;
        for (int y = 0; y < folder.height(); ++y) {
            const auto *plain = reinterpret_cast<const QRgb *>(folder.constScanLine(y));
            const auto *marked = reinterpret_cast<const QRgb *>(example.constScanLine(y));
            for (int x = 0; x < folder.width(); ++x) {
                if (difference(plain[x], marked[x]) <= kEmblemDifference)
                    continue;
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
                if (qAlpha(marked[x]) == 255)
                    ++colours[marked[x]];
            }
        }
        // An emblem is a mark on the folder, not another folder.
        if (right < 0 || (right - left) * (bottom - top) > folder.width() * folder.height() / 2)
            continue;
        centre += QPointF(left + right + 1, top + bottom + 1) / 2;
        side += std::max(right - left + 1, bottom - top + 1);
        ++found;
    }
    if (found == 0 || colours.isEmpty())
        return std::nullopt;
    centre /= found;
    side /= found;
    const auto commonest = std::max_element(colours.cbegin(), colours.cend());
    return Emblem{QRectF(centre.x() - side / 2, centre.y() - side / 2, side, side), QColor::fromRgba(commonest.key())};
}

/// Without examples: the middle of the folder's front, in its colour made
/// lighter (or darker, on a light folder).
Emblem guessedEmblem(const QImage &folder)
{
    const qreal side = folder.width() * 0.36;
    const QPointF centre(folder.width() * 0.5, folder.height() * 0.57);
    const QColor front = QColor::fromRgba(folder.pixel(centre.toPoint()));
    const QColor color = front.lightness() < 150 ? front.lighter(170) : front.darker(140);
    return Emblem{QRectF(centre.x() - side / 2, centre.y() - side / 2, side, side), color};
}

/// The folder icons of the system: the plain one and those with an emblem.
struct SystemFolders {
    QIcon folder;
    QList<QIcon> examples;
};

SystemFolders systemFolders()
{
    SystemFolders folders;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // The shell's icons: the special folders of the home wear their emblems.
    const QFileIconProvider provider;
    folders.folder = provider.icon(QFileIconProvider::Folder);
    for (const auto location : {QStandardPaths::MoviesLocation, QStandardPaths::PicturesLocation,
                                QStandardPaths::MusicLocation, QStandardPaths::DocumentsLocation}) {
        const QString path = QStandardPaths::writableLocation(location);
        if (QFileInfo(path).isDir())
            folders.examples.append(provider.icon(QFileInfo(path)));
    }
#else
    const QList<QIcon> icons = FolderIcon::themeIcons(
        {QStringLiteral("folder"), QStringLiteral("folder-videos"), QStringLiteral("folder-pictures"),
         QStringLiteral("folder-music"), QStringLiteral("folder-documents")});
    folders.folder = icons.constFirst();
    for (const QIcon &example : icons.mid(1))
        if (!example.isNull())
            folders.examples.append(example);
#endif
    return folders;
}

QString iconDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("folder-icon"));
}

} // namespace

namespace FolderIcon {

#if defined(Q_OS_WIN)
QByteArray icoOf(const QImage &image)
{
    QList<QByteArray> entries;
    const QList<int> sizes{16, 24, 32, 48, 64, 256};
    for (const int size : sizes) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG");
        entries.append(png);
    }
    QByteArray ico;
    QDataStream out(&ico, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << quint16(0) << quint16(1) << quint16(entries.size());
    quint32 offset = 6 + 16 * entries.size();
    for (int i = 0; i < entries.size(); ++i) {
        const quint8 side = sizes.at(i) >= 256 ? 0 : quint8(sizes.at(i));
        out << side << side << quint8(0) << quint8(0) << quint16(1) << quint16(32)
            << quint32(entries.at(i).size()) << offset;
        offset += entries.at(i).size();
    }
    for (const QByteArray &entry : entries)
        out.writeRawData(entry.constData(), int(entry.size()));
    return ico;
}
#endif

bool writeFile(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

} // namespace FolderIcon

namespace {

/// Sets `iconFile` as the icon of `folder`, unless the folder wears another
/// one the user gave it. True when the folder has it.
bool setFolderIcon(const QString &folder, const QString &iconFile, const QImage &image)
{
#if defined(Q_OS_WIN)
    const QString ini = QDir(folder).filePath(QStringLiteral("desktop.ini"));
    QFile existing(ini);
    if (existing.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = existing.readAll();
        const QString text = bytes.startsWith("\xff\xfe") ? QString::fromUtf16(reinterpret_cast<const char16_t *>(bytes.constData() + 2), (bytes.size() - 2) / 2)
                                                           : QString::fromLocal8Bit(bytes);
        if (text.contains(QLatin1String("IconResource"), Qt::CaseInsensitive)
            && !text.contains(QDir::toNativeSeparators(iconDirectory()), Qt::CaseInsensitive))
            return false;
        existing.close();
    }
    if (!FolderIcon::writeFile(iconFile, FolderIcon::icoOf(image)))
        return false;
    const QString content = QStringLiteral("[.ShellClassInfo]\r\nIconResource=%1,0\r\n").arg(QDir::toNativeSeparators(iconFile));
    QByteArray data("\xff\xfe", 2);
    data.append(reinterpret_cast<const char *>(content.utf16()), content.size() * 2);
    const std::wstring iniPath = QDir::toNativeSeparators(ini).toStdWString();
    SetFileAttributesW(iniPath.c_str(), FILE_ATTRIBUTE_NORMAL); // Hidden and system files refuse to be replaced.
    QFile file(ini);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size())
        return false;
    file.close();
    SetFileAttributesW(iniPath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    // The shell reads desktop.ini only in a read-only (or system) folder.
    const std::wstring folderPath = QDir::toNativeSeparators(folder).toStdWString();
    SetFileAttributesW(folderPath.c_str(), GetFileAttributesW(folderPath.c_str()) | FILE_ATTRIBUTE_READONLY);
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, folderPath.c_str(), nullptr);
    return true;
#elif defined(Q_OS_MACOS)
    if (!image.save(iconFile, "PNG"))
        return false;
    using Send = id (*)(id, SEL);
    using SendObject = id (*)(id, SEL, id);
    using SendText = id (*)(id, SEL, const char *);
    using SetIcon = BOOL (*)(id, SEL, id, id, unsigned long);
    const auto nsString = [](const QString &text) {
        return reinterpret_cast<SendText>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSString")),
                                                        sel_registerName("stringWithUTF8String:"), text.toUtf8().constData());
    };
    id picture = reinterpret_cast<Send>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSImage")), sel_registerName("alloc"));
    picture = reinterpret_cast<SendObject>(objc_msgSend)(picture, sel_registerName("initWithContentsOfFile:"), nsString(iconFile));
    if (!picture)
        return false;
    const id workspace = reinterpret_cast<Send>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSWorkspace")),
                                                              sel_registerName("sharedWorkspace"));
    const BOOL done = reinterpret_cast<SetIcon>(objc_msgSend)(workspace, sel_registerName("setIcon:forFile:options:"),
                                                              picture, nsString(folder), 0);
    reinterpret_cast<Send>(objc_msgSend)(picture, sel_registerName("release"));
    return done;
#else
    const QString url = QUrl::fromLocalFile(iconFile).toString();
    // GNOME (Nautilus) and the desktops built on GIO: the folder's metadata.
    QProcess gio;
    gio.start(QStringLiteral("gio"), {QStringLiteral("info"), QStringLiteral("-a"), QStringLiteral("metadata::custom-icon"), folder});
    if (gio.waitForFinished(2000)) {
        const QString info = QString::fromUtf8(gio.readAllStandardOutput());
        const qsizetype at = info.indexOf(QLatin1String("metadata::custom-icon:"));
        if (at >= 0 && !info.mid(at).section(QLatin1Char('\n'), 0, 0).contains(QUrl::fromLocalFile(iconDirectory()).toString()))
            return false;
    }
    if (!image.save(iconFile, "PNG"))
        return false;
    bool done = QProcess::execute(QStringLiteral("gio"), {QStringLiteral("set"), folder, QStringLiteral("metadata::custom-icon"), url}) == 0;
    // KDE (Dolphin): a .directory file in the folder.
    if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("KDE"), Qt::CaseInsensitive)) {
        const QString directory = QDir(folder).filePath(QStringLiteral(".directory"));
        QSettings entry(directory, QSettings::IniFormat);
        const QString icon = entry.value(QStringLiteral("Desktop Entry/Icon")).toString();
        if (icon.isEmpty() || icon.startsWith(iconDirectory())) {
            entry.setValue(QStringLiteral("Desktop Entry/Icon"), iconFile);
            done = true;
        }
    }
    return done;
#endif
}

} // namespace

namespace FolderIcon {

QImage imageOf(const QIcon &icon, int size)
{
    QImage image = icon.pixmap(size, size).toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (!image.isNull() && image.size() != QSize(size, size))
        image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
QList<QIcon> themeIcons(const QStringList &names)
{
    const QString themeName = QIcon::themeName();
    const QStringList searchPaths = QIcon::themeSearchPaths();
    if (QIcon::fromTheme(names.value(0)).isNull()) {
        // Without a desktop platform theme Qt knows no icon theme: GNOME's,
        // in the folders of the freedesktop specification.
        QStringList paths = searchPaths;
        paths.append(QDir::home().filePath(QStringLiteral(".icons")));
        paths.append(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"),
                                               QStandardPaths::LocateDirectory));
        QIcon::setThemeSearchPaths(paths);
        QProcess gsettings;
        gsettings.start(QStringLiteral("gsettings"),
                        {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("icon-theme")});
        if (gsettings.waitForFinished(2000))
            QIcon::setThemeName(QString::fromUtf8(gsettings.readAllStandardOutput()).trimmed().remove(QLatin1Char('\'')));
    }
    // Images are taken now, while the theme is the one asked for.
    QList<QIcon> icons;
    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        icons.append(icon.isNull() ? QIcon() : QIcon(icon.pixmap(256, 256)));
    }
    QIcon::setThemeName(themeName);
    QIcon::setThemeSearchPaths(searchPaths);
    return icons;
}
#endif

QPainterPath pawnPath()
{
    QPainterPath pawn;
    pawn.setFillRule(Qt::WindingFill);
    // The head.
    pawn.addEllipse(QPointF(0.5, 0.16), 0.15, 0.15);
    // The collar, wide and thick: what tells a pawn from a bishop or a ball.
    pawn.addRoundedRect(QRectF(0.25, 0.345, 0.50, 0.105), 0.05, 0.05);
    // The body, flaring to the base.
    QPainterPath body;
    body.moveTo(0.40, 0.48);
    body.lineTo(0.60, 0.48);
    body.cubicTo(0.61, 0.66, 0.70, 0.79, 0.77, 0.87);
    body.lineTo(0.23, 0.87);
    body.cubicTo(0.30, 0.79, 0.39, 0.66, 0.40, 0.48);
    body.closeSubpath();
    pawn.addPath(body);
    // The base.
    pawn.addRoundedRect(QRectF(0.18, 0.90, 0.64, 0.10), 0.035, 0.035);
    return pawn;
}

QPainterPath databasePath()
{
    // Drawn on a grid of 100 and scaled down: the boolean operations
    // flatten curves at the path's own scale.
    const qreal rx = 40, ry = 12, top = 13, bottom = 87;
    QPainterPath cylinder;
    cylinder.setFillRule(Qt::WindingFill);
    cylinder.addEllipse(QPointF(50, top), rx, ry);
    cylinder.addRect(QRectF(50 - rx, top, 2 * rx, bottom - top));
    cylinder.addEllipse(QPointF(50, bottom), rx, ry);
    cylinder = cylinder.simplified();
    // The rim of the lid: a curved gap under it.
    QPainterPath lower, upper;
    lower.addEllipse(QPointF(50, top + 7.5), rx, ry);
    upper.addEllipse(QPointF(50, top + 3.5), rx, ry);
    cylinder = cylinder.subtracted(lower.subtracted(upper));
    // The pawn cut out of the body.
    QTransform place;
    place.translate(50 - 23, 37);
    place.scale(46, 46);
    cylinder = cylinder.subtracted(place.map(pawnPath()));
    return QTransform::fromScale(0.01, 0.01).map(cylinder);
}

QImage compose(const QIcon &folder, const QList<QIcon> &examples, int size)
{
    QImage image = imageOf(folder, size);
    if (image.isNull())
        return {};
    QList<QImage> marked;
    for (const QIcon &example : examples)
        marked.append(imageOf(example, size));
    const Emblem emblem = emblemOf(image, marked).value_or(guessedEmblem(image));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    // As tall as the emblems, centred.
    const QRectF box = emblem.box.adjusted(emblem.box.width() * 0.04, 0, -emblem.box.width() * 0.04, 0);
    painter.translate(box.topLeft());
    painter.scale(box.width(), box.height());
    painter.fillPath(pawnPath(), emblem.color);
    return image;
}

void applyToChessFolders()
{
    if (UserFolders::isOverridden())
        return;
    QStringList folders;
    for (const QString &folder : {UserFolders::chessDir(), UserFolders::pragmaDir()})
        if (QFileInfo(folder).isDir() && !folders.contains(folder))
            folders.append(folder);
    if (folders.isEmpty())
        return;
#if defined(Q_OS_MACOS)
    const int size = 512;
#else
    const int size = 256;
#endif
    const SystemFolders system = systemFolders();
    const QImage image = compose(system.folder, system.examples, size);
    if (image.isNull())
        return;

    QByteArray png;
    {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
    }
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(png, QCryptographicHash::Sha256).toHex().left(12));
    // A new name for every new picture: file managers keep icons by path.
    QDir directory(iconDirectory());
    directory.mkpath(QStringLiteral("."));
#if defined(Q_OS_WIN)
    const QString suffix = QStringLiteral(".ico");
#else
    const QString suffix = QStringLiteral(".png");
#endif
    const QString iconFile = directory.filePath(QStringLiteral("chess-folder-") + hash + suffix);

    // "folder|hash" for each folder that has the picture of that hash.
    QSettings settings;
    QStringList applied = settings.value(QStringLiteral("folderIcon/applied")).toStringList();
    bool changed = false;
    for (const QString &folder : folders) {
        const QString entry = folder + QLatin1Char('|') + hash;
        if (applied.contains(entry) || !setFolderIcon(folder, iconFile, image))
            continue;
        applied.removeIf([&folder](const QString &old) { return old.section(QLatin1Char('|'), 0, -2) == folder; });
        applied.append(entry);
        changed = true;
    }
    if (!changed)
        return;
    settings.setValue(QStringLiteral("folderIcon/applied"), applied);
    for (const QString &old : directory.entryList({QStringLiteral("chess-folder-*")}, QDir::Files))
        if (directory.filePath(old) != iconFile)
            directory.remove(old);
}

} // namespace FolderIcon
