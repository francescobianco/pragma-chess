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
    out << YAML::Key << "ply" << YAML::Value << ply;
    out << YAML::EndMap;

    out << YAML::Key << "board" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "flipped" << YAML::Value << boardFlipped;
    out << YAML::Key << "coordinates" << YAML::Value << showCoordinates;
    out << YAML::EndMap;

    out << YAML::Key << "games" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "search" << YAML::Value << YAML::DoubleQuoted << toStd(gameSearch);
    out << YAML::EndMap;

    out << YAML::Key << "engine" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value;
    if (engineName.isEmpty())
        out << YAML::Null;
    else
        out << toStd(engineName);
    out << YAML::Key << "analyzing" << YAML::Value << engineAnalyzing;
    out << YAML::EndMap;

    out << YAML::Key << "workspace" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "layout" << YAML::Value << toStd(QString::fromLatin1(layout.toBase64()));
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

    const YAML::Node board = root["board"];
    env.boardFlipped = valueOf<bool>(board["flipped"], false);
    env.showCoordinates = valueOf<bool>(board["coordinates"], true);

    env.gameSearch = fromNode(root["games"]["search"]);

    const YAML::Node engine = root["engine"];
    env.engineName = fromNode(engine["name"]);
    env.engineAnalyzing = valueOf<bool>(engine["analyzing"], false);

    env.layout = QByteArray::fromBase64(fromNode(root["workspace"]["layout"]).toLatin1());
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
