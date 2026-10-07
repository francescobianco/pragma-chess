#include "Drawers.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QSet>

#include <yaml-cpp/yaml.h>

#include <string>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(Drawers)
};

QString text(const YAML::Node &node)
{
    try {
        return node && node.IsScalar() ? QString::fromStdString(node.as<std::string>()) : QString();
    } catch (const YAML::Exception &) {
        return QString();
    }
}

} // namespace

Drawers Drawers::read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? fromYaml(file.readAll()) : Drawers();
}

bool Drawers::write(const QString &path, const Drawers &drawers, QString *errorMessage)
{
    QByteArray existing;
    if (QFile file(path); file.open(QIODevice::ReadOnly))
        existing = file.readAll();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(drawers.toYaml(existing)) < 0 || !file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

Drawers Drawers::fromYaml(const QByteArray &yaml)
{
    Drawers drawers;
    try {
        const YAML::Node root = YAML::Load(yaml.toStdString());
        if (!root.IsMap() || !root["drawers"] || !root["drawers"].IsSequence())
            return drawers;
        for (const YAML::Node &node : root["drawers"]) {
            if (!node.IsMap())
                continue;
            drawers.list << Drawer{text(node["name"]).trimmed(), text(node["content"])};
        }
    } catch (const YAML::Exception &) {
    }
    return drawers;
}

QByteArray Drawers::toYaml(const QByteArray &existing) const
{
    YAML::Node root;
    try {
        root = YAML::Load(existing.toStdString());
    } catch (const YAML::Exception &) {
    }
    if (!root.IsMap())
        root = YAML::Node(YAML::NodeType::Map);
    root.remove("drawers");

    // The other keys as they were, then the drawers, their contents as
    // literal blocks so a game or a variation stays readable in the file.
    YAML::Emitter out;
    if (existing.trimmed().startsWith('#')) // The file's own heading comment.
        out << YAML::Comment(existing.trimmed().split('\n').constFirst().mid(1).trimmed().toStdString()) << YAML::Newline;
    out << YAML::BeginMap;
    for (const auto &entry : root) {
        out << YAML::Key << entry.first << YAML::Value << entry.second;
    }
    if (!list.isEmpty()) {
        out << YAML::Key << "drawers" << YAML::Value << YAML::BeginSeq;
        for (const Drawer &drawer : list) {
            out << YAML::BeginMap;
            out << YAML::Key << "name" << YAML::Value << drawer.name.trimmed().toStdString();
            out << YAML::Key << "content" << YAML::Value;
            if (drawer.content.contains(QLatin1Char('\n')))
                out << YAML::Literal;
            out << drawer.content.toStdString();
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }
    out << YAML::EndMap;
    return QByteArray(out.c_str()) + '\n';
}

QString Drawers::problem() const
{
    QSet<QString> names;
    for (const Drawer &drawer : list) {
        const QString name = drawer.name.trimmed();
        if (name.isEmpty())
            return Text::tr("Every drawer needs a name.");
        if (names.contains(name.toCaseFolded()))
            return Text::tr("Two drawers are called \"%1\": each needs a name of its own.").arg(name);
        names.insert(name.toCaseFolded());
    }
    return QString();
}
