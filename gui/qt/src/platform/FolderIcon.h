#pragma once

#include <QIcon>
#include <QImage>
#include <QPainterPath>
#include <QString>

/// The chess folder in the home (~/Chess, ~/Scacchi…) and the Pragma folder
/// in it wear a pawn, as
/// Videos wears a film and Pictures a picture: the system's own folder icon
/// with a pawn where the system draws the emblems of its folders, in their
/// colour. It is composed again at every start from the icons of the moment,
/// so it follows the icon theme (and its accent colour).
namespace FolderIcon {

/// The pawn, a silhouette in the unit square, in parts with gaps between
/// them as a figurine has: the head, the collar, the body and the base.
/// Shared with the icon of project files (ProjectFileType).
QPainterPath pawnPath();

/// The icons of the desktop's icon theme by freedesktop name (null where
/// the theme has none), as images of up to 256 pixels; found through GNOME's
/// setting when Qt knows no theme. Not on Windows and macOS.
QList<QIcon> themeIcons(const QStringList &names);

/// Writes a whole file, replacing it only once written.
bool writeFile(const QString &path, const QByteArray &data);

#if defined(Q_OS_WIN)
/// An .ico holding the image as PNG entries (Windows Vista and later read them).
QByteArray icoOf(const QImage &image);
#endif

/// The icon as an image `size` pixels square (premultiplied ARGB).
QImage imageOf(const QIcon &icon, int size);

/// The folder icon with a pawn on it, `size` pixels square: the emblem's
/// place and colour are read from `examples` (folders of the same theme
/// that carry an emblem, e.g. Videos, Pictures) against the plain `folder`.
/// A null image when there is no folder icon.
QImage compose(const QIcon &folder, const QList<QIcon> &examples, int size);

/// Puts the pawn on the chess folder and the Pragma folder, each if it
/// exists and wears no icon of the user's; only when the icon changed since
/// the last time, so a user who took it off is not overruled at every start.
void applyToChessFolders();

} // namespace FolderIcon
