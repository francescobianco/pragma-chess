#pragma once

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QStringList>

#include <optional>

/// Everything that makes up what the user is looking at: the open database,
/// game and move, board orientation, engine and window layout.
///
/// Projects are saved as `.pch` files (YAML) from the File menu, and the
/// same representation is used to restore the last session on startup.
struct Project {
    static constexpr int formatVersion = 1;
    static constexpr char fileSuffix[] = "pch";

    /// Absolute path of the database file.
    QString databasePath;
    /// Database id of the open game, or -1 when viewing a position without a game.
    qint64 gameId = -1;
    int ply = 0;
    /// Starting position when no game is open (empty = standard position).
    QString startFen;
    /// Moves (UCI) of a game that is not in the database, e.g. a new game.
    QStringList moves;

    bool boardFlipped = false;
    bool showCoordinates = true;

    /// UCI engine selected for analysis (empty = none) and whether it is running.
    QString engineName;
    bool engineAnalyzing = false;

    /// Dock and toolbar layout, as produced by QMainWindow::saveState().
    QByteArray layout;

    /// Serializes to YAML. Paths inside `baseDir` are written relative to it,
    /// so a .pch file can travel together with its databases.
    QString toYaml(const QDir &baseDir = QDir()) const;
    static std::optional<Project> fromYaml(const QString &yaml, const QDir &baseDir,
                                               QString *errorMessage);

    bool saveToFile(const QString &path, QString *errorMessage) const;
    static std::optional<Project> loadFromFile(const QString &path, QString *errorMessage);
};
