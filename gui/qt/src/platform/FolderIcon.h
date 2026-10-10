#pragma once

#include <QIcon>
#include <QImage>
#include <QString>

/// The chess folder in the home (~/Chess, ~/Scacchi…) wears a king, as
/// Videos wears a film and Pictures a picture: the system's own folder icon
/// with a king where the system draws the emblems of its folders, in their
/// colour. It is composed again at every start from the icons of the moment,
/// so it follows the icon theme (and its accent colour).
namespace FolderIcon {

/// The folder icon with a king on it, `size` pixels square: the emblem's
/// place and colour are read from `examples` (folders of the same theme
/// that carry an emblem, e.g. Videos, Pictures) against the plain `folder`.
/// A null image when there is no folder icon.
QImage compose(const QIcon &folder, const QList<QIcon> &examples, int size);

/// Puts the king on the chess folder, if it exists and wears no icon of
/// the user's; only when the icon changed since the last time, so a user
/// who took it off is not overruled at every start.
void applyToChessFolder();

} // namespace FolderIcon
