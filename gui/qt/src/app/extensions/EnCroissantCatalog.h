#pragma once

#include "Extension.h"

#include <QByteArray>

/// The catalogs En Croissant publishes on its site and its application reads
/// (docs/tech/downloads.md): engines, as links to their authors' own
/// downloads, and databases it converted into its own format. Pure,
/// unit-tested with the catalogs as served.
namespace EnCroissantCatalog {

QString enginesUrl(const QString &system, bool bmi2);
QString databasesUrl();
QString puzzlesUrl();

/// The engines of `system` with or without BMI2, newest first by name.
QList<Extension> engines(const QByteArray &json, const QString &system, bool bmi2);
/// Its databases (or puzzle databases): listed, not installable — they are in
/// En Croissant's own SQLite format, which Pragma Chess does not read yet.
QList<Extension> databases(const QByteArray &json, Extension::Kind kind);

} // namespace EnCroissantCatalog
