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
/// `PRAGMA_CHESS_DIR` overrides the chess folder (useful for testing).
namespace UserFolders {

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
