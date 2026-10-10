#pragma once

#include <QIcon>
#include <QImage>

/// Projects (.pch) are documents of Pragma Chess for the system: they open
/// with it, and their icon is the system's document with a pawn on the page,
/// where the theme writes its lines, in their colour — composed again at
/// every start, so it follows the icon theme.
///
/// Linux: a MIME type (`application/x-pragma-chess-project`, also installed
/// with the packages, data/*.mime.xml), its icon in the user's hicolor
/// theme, and Pragma Chess as its default application when there is none.
/// Windows: the ProgId the installer registers, for the user, with the icon
/// and this executable. macOS: the bundle declares the type (Info.plist), and
/// Finder draws its documents.
namespace ProjectFileType {

inline constexpr char mimeType[] = "application/x-pragma-chess-project";

/// `document` (the theme's plain text file) with its lines taken off and a
/// pawn in their place and colour; a null image when there is no document.
QImage compose(const QIcon &document, int size);

/// Registers the type, its icon and the application with the desktop, when
/// any of it changed since the last time.
void registerWithSystem();

} // namespace ProjectFileType
