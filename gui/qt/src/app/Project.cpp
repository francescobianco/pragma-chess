#include "Project.h"

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

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage)
        *errorMessage = message;
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

    out << YAML::Key << "database" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "path" << YAML::Value;
    if (database.isEmpty())
        out << YAML::Null;
    else
        out << toStd(database);
    out << YAML::EndMap;

    out << YAML::Key << "game" << YAML::Value << YAML::BeginMap;
    if (gameId >= 0)
        out << YAML::Key << "id" << YAML::Value << static_cast<long long>(gameId);
    else if (!startFen.isEmpty())
        out << YAML::Key << "fen" << YAML::Value << toStd(startFen);
    if (gameId < 0 && !moves.isEmpty())
        out << YAML::Key << "moves" << YAML::Value << toStd(moves.join(QLatin1Char(' ')));
    if (gameId < 0 && !moves.isEmpty() && !annotations.isEmpty())
        out << YAML::Key << "annotations" << YAML::Value << toStd(annotations.join(QLatin1Char(' ')));
    if (gameId < 0 && !variations.isEmpty())
        out << YAML::Key << "variations" << YAML::Value << toStd(variations);
    out << YAML::Key << "ply" << YAML::Value << ply;
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
    const QString database = fromNode(root["database"]["path"]);
    if (!database.isEmpty())
        env.databasePath = QDir::cleanPath(baseDir.absoluteFilePath(database));

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
