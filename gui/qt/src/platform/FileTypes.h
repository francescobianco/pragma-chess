#pragma once

#include <QIcon>
#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QString>

/// Projects (.pch) and databases (.pdb) are documents of Pragma Chess for
/// the system: they open with it, and their icon is the system's document
/// with a mark on the page — a pawn for a project, a database with a pawn
/// for a database — where the theme writes its lines, in their colour;
/// composed again at every start, so it follows the icon theme.
///
/// Linux: the MIME types (data/<app id>.mime.xml, installed with the
/// packages and also written for the user), their icons in the user's
/// hicolor theme, and Pragma Chess as their default application where there
/// is none. Windows: the installer's ProgIds, for the user, with the icons
/// and this executable. macOS: the bundle declares the types (Info.plist),
/// and Finder draws their documents.
namespace FileTypes {

/// A kind of document of Pragma Chess.
struct Type {
    QString mimeType;
    /// The icon's name in the hicolor theme (Linux).
    QString iconName;
    /// The class of the extension (Windows).
    QString progId;
    /// ".pch"
    QString extension;
    QString description;
    /// What is drawn on the page, in the unit square.
    QPainterPath mark;
};

/// Projects, then databases.
QList<Type> types();

/// `document` (the theme's plain text file) with its lines taken off and
/// `mark` in their place and colour; a null image when there is no document.
QImage compose(const QIcon &document, const QPainterPath &mark, int size);

/// Registers the types, their icons and the application with the desktop,
/// when any of it changed since the last time.
void registerWithSystem();

} // namespace FileTypes
