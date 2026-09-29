#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QSettings;

/// One engine the user can pick: a UCI executable and how to run it.
struct EngineProfile {
    QString id;
    QString name;
    /// Executable; empty for the bundled engine, whose path is found at run time.
    QString path;
    bool bundled = false;
    /// Search threads, 0 = automatic (half the cores).
    int threads = 0;
    /// Hash table in MB, 0 = the engine's default.
    int hashMb = 0;
};

/// A UCI engine found on this computer by EngineDetector.
struct DetectedEngine {
    QString path;
    /// "id name" from the UCI handshake, or a name made from the file name.
    QString name;
};

/// The engines of this computer (user settings, not the project: paths are
/// per machine). The engine shipped with Pragma Chess is always there, first,
/// and cannot be removed; the others are added by hand or by detection.
class EngineCatalog {
public:
    static const QString kBundledId;

    /// A catalog holding only the bundled engine.
    EngineCatalog();

    static EngineCatalog load(QSettings &settings);
    void save(QSettings &settings) const;

    const QList<EngineProfile> &engines() const { return m_engines; }
    /// The engine with this id, or nullptr.
    const EngineProfile *find(const QString &id) const;
    /// The engine a project refers to: by id, else by name or executable name
    /// (projects before engine ids stored "stockfish"), else the bundled one.
    /// Projects travel between computers, so an unknown id is not an error.
    const EngineProfile &resolve(const QString &id, const QString &name = {}) const;

    /// Adds an engine with a new id and returns that id.
    QString add(EngineProfile profile);
    /// Replaces the engine with the same id; the bundled engine keeps its path.
    void update(const EngineProfile &profile);
    /// Removes an engine; the bundled one cannot be removed.
    bool remove(const QString &id);

    /// Adds the detected engines whose executable is not in the catalog yet
    /// (the same file reached through a symlink or another folder counts as
    /// the same engine) and returns how many were added.
    /// @p bundledPath is where the bundled engine was found, so that detection
    /// does not add it a second time.
    int addDetected(const QList<DetectedEngine> &found, const QString &bundledPath);

    /// The canonical form of an executable path, symlinks resolved, used to
    /// compare engines. Falls back to the cleaned absolute path if the file is missing.
    static QString canonicalPath(const QString &path);

    /// The name of the bundled engine ("Stockfish 19").
    static QString bundledEngineName();
    /// Where the bundled engine is, or empty if this build has none: next to
    /// the application (Windows, macOS, a build with `make stockfish`) or in
    /// lib/pragma-chess/engines (Linux packages). $PRAGMA_ENGINES_DIR overrides.
    static QString bundledEnginePath();
    /// The folders bundledEnginePath() looks in, most specific first.
    static QStringList bundledEngineDirs(const QString &applicationDir);
    /// The executable to run for @p profile, empty if there is none. A build
    /// without the bundled engine (development) falls back to a "stockfish"
    /// installed on the system.
    static QString executableFor(const EngineProfile &profile);

private:
    QList<EngineProfile> m_engines;
};
