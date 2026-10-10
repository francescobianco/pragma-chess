#pragma once

#include <QString>

/// The archives extensions come in (Help ▸ Manage Extensions): the engines'
/// authors publish .zip, .tar and .tar.gz. Opened with miniz; nothing is
/// written outside the folder given (an entry with "..", or an absolute one,
/// is refused).
namespace Archive {

enum class Kind { None, Zip, Tar, TarGz };

/// The kind by the name's ending (a URL's query and fragment left out).
Kind kindOf(const QString &name);

/// Extracts `file` into `folder` (made if missing). A file that is no
/// archive (Kind::None) is copied there under `plainName`.
bool extract(const QString &file, Kind kind, const QString &folder, QString *error,
             const QString &plainName = QString());

} // namespace Archive
