#pragma once

#include "BoardState.h"
#include "Chapters.h"
#include "EngineEvaluation.h"
#include "WorkspaceLayout.h"

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QStringList>

#include <optional>

/// Everything that makes up what the user is looking at: the open database,
/// game and move, board orientation, engine and the panels (WorkspaceLayout).
///
/// Projects are saved as `.pch` files (YAML) from the File menu, and the
/// same representation is used to restore the last session on startup.
struct Project {
    /// 2: the chapters. 3: the texts by language (LocalizedText).
    static constexpr int formatVersion = 3;
    static constexpr char fileSuffix[] = "pch";

    /// The project's own name (File ▸ Project Information…), shown in the title
    /// bar in place of the file's; empty for the file's. Like the chapters'
    /// titles and the paragraphs, in each language the project is written in.
    LocalizedText name;
    /// Its texts are written in several languages, one chosen at a time in
    /// Project Information; otherwise they are shown and written in the
    /// interface's. Written `multilingual: true`, only when on.
    bool multilingual = false;
    /// Read-only: a flag against changes made without thinking, not a lock —
    /// unticked in Project Information, the project is changed as any other. The
    /// projects we distribute come with it. Written `read-only: true`, only
    /// when on.
    bool readOnly = false;
    /// Absolute path of the database file.
    QString databasePath;
    /// The database's lineage (DatabaseProperties::id), so the project finds
    /// it wherever it is — moved, renamed, on another computer: the database
    /// with this lineage in the Databases folder is opened when the path has
    /// none or another. The projects we distribute name their database so,
    /// with no path. Written `database: lineage:`, only when known.
    QString databaseLineage;
    /// The chapters, their games and paragraphs, and the one open (where in
    /// it is the chapter's currentGame and ply). Empty for a project written
    /// before chapters: the fields below say what its one game was.
    QList<Chapter> chapters;
    int chapter = 0;
    /// No chapter yet (ChapterBook::hasChapters): `chapters` holds the one
    /// game on the board.
    bool noChapters = false;
    /// The chapters came by themselves (ChapterBook::isAutomatic): written
    /// `automatic`; a project without the key has them so when it holds one
    /// chapter under the default title.
    bool automaticChapters = false;
    /// Before chapters (read, never written): database id of the open game,
    /// or -1 when viewing a position without a game.
    qint64 gameId = -1;
    int ply = 0;
    /// Starting position when no game is open (empty = standard position).
    QString startFen;
    /// Moves (UCI) of a game that is not in the database, e.g. a new game.
    QStringList moves;
    /// The annotations of those moves, as "<ply>:<suffix>" with the suffix of
    /// MoveAnnotation::storedSuffix: "3:!", "12:??$18".
    QStringList annotations;
    /// Their variations, as GameVariations::toText writes them.
    QString variations;

    bool boardFlipped = false;
    bool showCoordinates = true;

    /// Engine selected for analysis (an EngineCatalog id; empty = the default)
    /// and whether it is running. engineName is its name, for people reading
    /// the file and for projects written before engine ids.
    QString engineId;
    QString engineName;
    bool engineAnalyzing = false;

    /// Training Mode: the engine answers as the other colour. A project closed
    /// while training opens training, with the user on `trainingSide`.
    bool training = false;
    Side trainingSide = Side::White;
    /// The tutor stopped the game on the user's move at `ply`: the engine's
    /// answer it holds back (`reply`, UCI), the alert (`alert`: blunder,
    /// mistake, inaccuracy, missed-chance), the evaluation the move was
    /// judged from (`before`) and the search for the answer (`after`). A
    /// project closed with the alert up opens with it up, the engine waiting.
    struct TutorHold {
        int ply = 0;
        QString reply;
        QString alert;
        EngineEvaluation before;
        EngineEvaluation after;
    };
    std::optional<TutorHold> tutorHold;
    /// A game of the lobby on the board (Lobby Mode): its room's id and its
    /// players' keys, empty when there is none; `lobbyMode` whether the mode
    /// was on. Written only with a lobby game.
    QString lobbyRoom;
    QString lobbyWhite;
    QString lobbyBlack;
    bool lobbyMode = false;
    /// Explain was on for the move on the board: it comes back on with the
    /// project. Written only when on.
    bool explain = false;

    /// Which panels are shown and how the space is shared (`workspace`).
    WorkspaceLayout workspace;
    /// The opaque `layout` blob (QMainWindow::saveState) of projects written
    /// before the shares: read, applied once, never written again.
    QByteArray legacyLayout;

    /// Serializes to YAML. Paths inside `baseDir` are written relative to it,
    /// so a .pch file can travel together with its databases.
    QString toYaml(const QDir &baseDir = QDir()) const;
    /// The texts of a project written before languages (format 2 and
    /// earlier) are read as written in `legacyLanguage`.
    static std::optional<Project> fromYaml(const QString &yaml, const QDir &baseDir, QString *errorMessage,
                                           const QString &legacyLanguage = QStringLiteral("en"));

    bool saveToFile(const QString &path, QString *errorMessage) const;
    static std::optional<Project> loadFromFile(const QString &path, QString *errorMessage,
                                               const QString &legacyLanguage = QStringLiteral("en"));
};
