#pragma once

#include <QList>
#include <QString>

/// A thing Help ▸ Manage Extensions can install: an engine, a database.
/// Installing only puts files where Pragma Chess looks and configures them
/// (an engine joins Manage Engines): nothing of the system is touched.
struct Extension {
    enum class Kind { Engine, Database, Puzzles };

    /// Stable within its provider ("stockfish-19-linux-bmi2").
    QString id;
    Kind kind = Kind::Engine;
    QString name;
    QString version;
    QString description;
    /// Where it is downloaded from: an archive (.zip, .tar.gz) or a file.
    QString downloadUrl;
    /// For an engine, its executable inside the archive.
    QString executable;
    qint64 downloadSize = 0;
    int elo = 0;
    /// For a database, its games or puzzles.
    qint64 count = 0;
    /// Who made it and on what terms, as far as the provider says.
    QString author;
    QString license;
    /// Why it cannot be installed (yet); empty when it can.
    QString unavailable;

    bool installable() const { return unavailable.isEmpty() && !downloadUrl.isEmpty(); }
};

/// What this computer is, for the catalogs that offer a build per system and
/// processor: "linux", "windows" or "macos", and whether the processor has
/// BMI2 (engines built for it are faster).
namespace ThisComputer {
QString system();
bool hasBmi2();
} // namespace ThisComputer
