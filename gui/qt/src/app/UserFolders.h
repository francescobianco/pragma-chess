#pragma once

#include <QLocale>
#include <QString>

/// Where Pragma Chess keeps user data, following the convention of the
/// localized folders in the home directory (Desktop, Documents, …):
///
///     ~/Chess/Pragma/Databases        (English)
///     ~/Scacchi/Pragma/Databases      (Italian)
///     ~/Scacchi/Pragma/Projects       (.pch files)
///     ~/Scacchi/Pragma/Books          (Polyglot opening books, .bin)
///     ~/Scacchi/Pragma/Books/Opening Names  (opening names databases, .pdb)
///
/// An existing chess folder is reused even if the language changes later.
/// Options ▸ Folder Settings… moves any of them elsewhere (`FolderChoice`,
/// per computer, used from the next start). `PRAGMA_CHESS_DIR` overrides the
/// chess folder and ignores those choices (useful for testing).
namespace UserFolders {

/// The folders the user chose; an empty one is the default, inside the
/// Pragma folder (Opening Names inside Books).
struct FolderChoice {
    QString pragma;
    QString databases;
    QString projects;
    QString books;
    QString openingNames;

    bool operator==(const FolderChoice &) const = default;
};

/// The folders as they are with `choice`, given the default Pragma folder.
struct Folders {
    QString pragma;
    QString databases;
    QString projects;
    QString books;
    QString openingNames;
};
Folders resolve(const FolderChoice &choice, const QString &defaultPragma);

/// The Pragma folder when none is chosen: "Pragma" in the chess folder.
QString defaultPragmaDir();
/// Whether `PRAGMA_CHESS_DIR` decides, so the choices are ignored.
bool isOverridden();
/// The choices stored in the user's settings, and storing new ones: they are
/// read once, so they take effect the next time Pragma Chess starts.
FolderChoice chosenFolders();
void setChosenFolders(const FolderChoice &choice);

/// Name of the chess folder for a language, e.g. "Scacchi" for Italian.
QString chessFolderName(const QLocale &locale);

QString chessDir();
QString pragmaDir();
QString databasesDir();
QString projectsDir();
QString booksDir();
/// The opening names databases (type Opening Book): kept with the books, not
/// among the databases of games.
QString openingNamesDir();

/// Create the folders if needed. Return false on failure.
bool ensureDatabasesDir();
bool ensureProjectsDir();
bool ensureBooksDir();
bool ensureOpeningNamesDir();

inline constexpr char databaseSuffix[] = "pdb";
inline constexpr char bookSuffix[] = "bin";

} // namespace UserFolders
