#include "Project.h"

#include "GameVariations.h"
#include "Pgn.h"
#include "MoveComment.h"
#include "MoveAnnotation.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <yaml-cpp/yaml.h>

namespace {

std::string toStd(const QString &value)
{
    return value.toStdString();
}

QString fromNode(const YAML::Node &node, const QString &fallback = QString())
{
    if (!node || node.IsNull() || !node.IsScalar())
        return fallback;
    return QString::fromStdString(node.as<std::string>());
}

template<typename T>
T valueOf(const YAML::Node &node, T fallback)
{
    if (!node || node.IsNull() || !node.IsScalar())
        return fallback;
    try {
        return node.as<T>();
    } catch (const YAML::Exception &) {
        return fallback;
    }
}

/// A game of a chapter: its uid when it is stored, and its content in any
/// case, so the chapter can be shown without the database.
void writeGame(YAML::Emitter &out, const ChapterGame &entry)
{
    const GameRecord &game = entry.game;
    out << YAML::BeginMap;
    const auto text = [&out](const char *key, const QString &value) {
        if (!value.isEmpty())
            out << YAML::Key << key << YAML::Value << toStd(value);
    };
    text("uid", game.uid);
    text("white", game.white);
    text("black", game.black);
    if (game.whiteElo > 0)
        out << YAML::Key << "whiteElo" << YAML::Value << game.whiteElo;
    if (game.blackElo > 0)
        out << YAML::Key << "blackElo" << YAML::Value << game.blackElo;
    text("event", game.event);
    text("site", game.site);
    text("date", game.date);
    text("round", game.round);
    text("result", game.result);
    text("eco", game.eco);
    text("fen", game.startFen);
    QStringList moves;
    QStringList annotations;
    for (const MoveRecord &move : game.moves) {
        moves << move.uci;
        if (!move.nags.isEmpty())
            annotations << QStringLiteral("%1:%2").arg(moves.size()).arg(MoveAnnotation::storedSuffix(move.nags));
    }
    text("moves", moves.join(QLatin1Char(' ')));
    text("annotations", annotations.join(QLatin1Char(' ')));
    text("variations", GameVariations::toText(game.variations));
    text("tags", Pgn::tagsText(game.tags));
    text("comments", MoveComment::toJson(game));
    if (!entry.paragraphs.isEmpty()) {
        out << YAML::Key << "paragraphs" << YAML::Value << YAML::BeginSeq;
        for (const Paragraph &paragraph : entry.paragraphs) {
            out << YAML::BeginMap;
            out << YAML::Key << "ply" << YAML::Value << paragraph.ply;
            out << YAML::Key << "text" << YAML::Value << YAML::Literal << toStd(paragraph.text);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }
    out << YAML::EndMap;
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage)
        *errorMessage = message;
}

ChapterGame readGame(YAML::Node node)
{
    ChapterGame entry;
    GameRecord &game = entry.game;
    game.uid = fromNode(node["uid"]);
    game.white = fromNode(node["white"]);
    game.black = fromNode(node["black"]);
    game.whiteElo = valueOf<int>(node["whiteElo"], 0);
    game.blackElo = valueOf<int>(node["blackElo"], 0);
    game.event = fromNode(node["event"]);
    game.site = fromNode(node["site"]);
    game.date = fromNode(node["date"]);
    game.round = fromNode(node["round"]);
    game.result = fromNode(node["result"]);
    game.eco = fromNode(node["eco"]);
    game.startFen = fromNode(node["fen"]);
    for (const QString &uci : fromNode(node["moves"]).split(QLatin1Char(' '), Qt::SkipEmptyParts))
        game.moves << MoveRecord{QString(), uci, {}}; // SAN is filled in when the game is resolved.
    for (const QString &annotation : fromNode(node["annotations"]).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const int ply = annotation.section(QLatin1Char(':'), 0, 0).toInt();
        if (ply >= 1 && ply <= game.moves.size())
            MoveAnnotation::split(annotation.section(QLatin1Char(':'), 1), &game.moves[ply - 1].nags);
    }
    game.variations = GameVariations::fromText(fromNode(node["variations"]));
    game.tags = Pgn::tagsFromText(fromNode(node["tags"]));
    MoveComment::fromJson(game, fromNode(node["comments"]));
    game.plyCount = int(game.moves.size());
    YAML::Node paragraphs = node["paragraphs"];
    if (paragraphs.IsSequence()) {
        for (YAML::Node paragraph : paragraphs) {
            QString text = fromNode(paragraph["text"]);
            while (text.endsWith(QLatin1Char('\n'))) // The literal block's own line end.
                text.chop(1);
            if (!text.trimmed().isEmpty())
                entry.paragraphs << Paragraph{qMax(0, valueOf<int>(paragraph["ply"], 0)), text};
        }
    }
    return entry;
}

} // namespace

QString Project::toYaml(const QDir &baseDir) const
{
    QString database = databasePath;
    if (!database.isEmpty() && baseDir != QDir()) {
        const QString relative = baseDir.relativeFilePath(database);
        if (!relative.startsWith(QLatin1String("..")))
            database = relative;
    }

    YAML::Emitter out;
    out.SetIndent(2);
    out << YAML::Comment("Pragma Chess project");
    out << YAML::BeginMap;
    out << YAML::Key << "pragma-chess" << YAML::Value << formatVersion;
    if (!name.isEmpty())
        out << YAML::Key << "name" << YAML::Value << toStd(name);

    out << YAML::Key << "database" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "path" << YAML::Value;
    if (database.isEmpty())
        out << YAML::Null;
    else
        out << toStd(database);
    out << YAML::EndMap;

    // The chapters: each a list of games with the paragraphs between their moves.
    out << YAML::Key << "chapters" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "current" << YAML::Value << chapter;
    out << YAML::Key << "list" << YAML::Value << YAML::BeginSeq;
    for (const Chapter &entry : chapters) {
        out << YAML::BeginMap;
        out << YAML::Key << "title" << YAML::Value << toStd(entry.title);
        out << YAML::Key << "game" << YAML::Value << entry.currentGame;
        out << YAML::Key << "ply" << YAML::Value << entry.ply;
        out << YAML::Key << "games" << YAML::Value << YAML::BeginSeq;
        for (const ChapterGame &game : entry.games)
            writeGame(out, game);
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;

    out << YAML::Key << "board" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "flipped" << YAML::Value << boardFlipped;
    out << YAML::Key << "coordinates" << YAML::Value << showCoordinates;
    out << YAML::EndMap;

    out << YAML::Key << "engine" << YAML::Value << YAML::BeginMap;
    if (!engineId.isEmpty())
        out << YAML::Key << "id" << YAML::Value << toStd(engineId);
    out << YAML::Key << "name" << YAML::Value;
    if (engineName.isEmpty())
        out << YAML::Null;
    else
        out << toStd(engineName);
    out << YAML::Key << "analyzing" << YAML::Value << engineAnalyzing;
    out << YAML::EndMap;

    // Only while training: the section itself is the flag.
    if (training) {
        out << YAML::Key << "training" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "side" << YAML::Value << (trainingSide == Side::Black ? "black" : "white");
        out << YAML::EndMap;
    }

    // The panels: which are shown, and the shares of the usable area, in per cent.
    out << YAML::Key << "workspace" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "panels" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "toolbar" << YAML::Value << workspace.toolbar;
    out << YAML::Key << "moves" << YAML::Value << workspace.moves;
    out << YAML::Key << "openingTree" << YAML::Value << workspace.openingTree;
    out << YAML::Key << "engine" << YAML::Value << workspace.engine;
    out << YAML::Key << "games" << YAML::Value << workspace.games;
    out << YAML::EndMap;
    out << YAML::Key << "gamesHeight" << YAML::Value << workspace.gamesHeight;
    out << YAML::Key << "movesWidth" << YAML::Value << workspace.movesWidth;
    out << YAML::Key << "engineHeight" << YAML::Value << workspace.engineHeight;
    out << YAML::Key << "treeWidth" << YAML::Value << workspace.treeWidth;
    out << YAML::EndMap;

    out << YAML::EndMap;
    return QString::fromStdString(out.c_str()) + QLatin1Char('\n');
}

std::optional<Project> Project::fromYaml(const QString &yaml, const QDir &baseDir,
                                                 QString *errorMessage)
{
    YAML::Node root;
    try {
        root = YAML::Load(yaml.toStdString());
    } catch (const YAML::Exception &e) {
        setError(errorMessage, QString::fromStdString(e.what()));
        return std::nullopt;
    }
    if (!root.IsMap() || !root["pragma-chess"]) {
        setError(errorMessage, QObject::tr("Not a Pragma Chess project."));
        return std::nullopt;
    }
    if (valueOf<int>(root["pragma-chess"], 0) > formatVersion) {
        setError(errorMessage, QObject::tr("The project was created by a newer version of Pragma Chess."));
        return std::nullopt;
    }

    Project env;
    env.name = fromNode(root["name"]).trimmed();
    const QString database = fromNode(root["database"]["path"]);
    if (!database.isEmpty())
        env.databasePath = QDir::cleanPath(baseDir.absoluteFilePath(database));

    YAML::Node chapters = root["chapters"];
    if (chapters.IsMap() && chapters["list"].IsSequence()) {
        for (YAML::Node node : chapters["list"]) {
            Chapter entry;
            entry.title = fromNode(node["title"]);
            entry.games.clear();
            if (node["games"].IsSequence()) {
                for (YAML::Node game : node["games"])
                    entry.games << readGame(game);
            }
            if (entry.games.isEmpty())
                entry.games << ChapterGame();
            entry.currentGame = qBound(0, valueOf<int>(node["game"], 0), int(entry.games.size()) - 1);
            entry.ply = qMax(0, valueOf<int>(node["ply"], 0));
            env.chapters << entry;
        }
        env.chapter = env.chapters.isEmpty() ? 0 : qBound(0, valueOf<int>(chapters["current"], 0), int(env.chapters.size()) - 1);
    }

    const YAML::Node game = root["game"];
    env.gameId = valueOf<long long>(game["id"], -1);
    env.ply = qMax(0, valueOf<int>(game["ply"], 0));
    env.startFen = fromNode(game["fen"]);
    env.moves = fromNode(game["moves"]).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    env.annotations = fromNode(game["annotations"]).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    env.variations = fromNode(game["variations"]);

    const YAML::Node board = root["board"];
    env.boardFlipped = valueOf<bool>(board["flipped"], false);
    env.showCoordinates = valueOf<bool>(board["coordinates"], true);

    const YAML::Node engine = root["engine"];
    env.engineId = fromNode(engine["id"]);
    env.engineName = fromNode(engine["name"]);
    env.engineAnalyzing = valueOf<bool>(engine["analyzing"], false);

    const YAML::Node training = root["training"];
    env.training = training.IsMap();
    env.trainingSide = fromNode(training["side"]) == QLatin1String("black") ? Side::Black : Side::White;

    // Looked up through non-const nodes, as the rest of the file does: a
    // const lookup of a missing key gives a node that throws when read.
    YAML::Node workspace = root["workspace"];
    YAML::Node panels = workspace["panels"];
    const bool hasShares = panels.IsMap();
    const WorkspaceLayout defaults;
    env.workspace.toolbar = valueOf<bool>(panels["toolbar"], defaults.toolbar);
    env.workspace.moves = valueOf<bool>(panels["moves"], defaults.moves);
    env.workspace.openingTree = valueOf<bool>(panels["openingTree"], defaults.openingTree);
    env.workspace.engine = valueOf<bool>(panels["engine"], defaults.engine);
    env.workspace.games = valueOf<bool>(panels["games"], defaults.games);
    env.workspace.gamesHeight = WorkspaceLayout::clamped(valueOf<double>(workspace["gamesHeight"], defaults.gamesHeight));
    env.workspace.movesWidth = WorkspaceLayout::clamped(valueOf<double>(workspace["movesWidth"], defaults.movesWidth));
    env.workspace.engineHeight = WorkspaceLayout::clamped(valueOf<double>(workspace["engineHeight"], defaults.engineHeight));
    env.workspace.treeWidth = WorkspaceLayout::clamped(valueOf<double>(workspace["treeWidth"], defaults.treeWidth));
    // Older projects carry Qt's opaque state instead; it is honoured once.
    if (!hasShares)
        env.legacyLayout = QByteArray::fromBase64(fromNode(workspace["layout"]).toLatin1());
    return env;
}

bool Project::saveToFile(const QString &path, QString *errorMessage) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, file.errorString());
        return false;
    }
    file.write(toYaml(QFileInfo(path).absoluteDir()).toUtf8());
    if (!file.commit()) {
        setError(errorMessage, file.errorString());
        return false;
    }
    return true;
}

std::optional<Project> Project::loadFromFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage, file.errorString());
        return std::nullopt;
    }
    return fromYaml(QString::fromUtf8(file.readAll()), QFileInfo(path).absoluteDir(), errorMessage);
}
