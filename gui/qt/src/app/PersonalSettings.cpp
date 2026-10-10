#include "PersonalSettings.h"

#include "UserFolders.h"
#include "lobby/RoomName.h"
#include "sync/SyncManifest.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <yaml-cpp/yaml.h>

#include <string>

namespace {

QString text(const YAML::Node &node)
{
    try {
        return node && node.IsScalar() ? QString::fromStdString(node.as<std::string>()).trimmed() : QString();
    } catch (const YAML::Exception &) {
        return QString();
    }
}

} // namespace

QString PersonalSettings::path()
{
    return QDir(UserFolders::pragmaDir()).filePath(QLatin1String(SyncManifest::personalFileName));
}

PersonalSettings PersonalSettings::read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? fromYaml(file.readAll()) : PersonalSettings();
}

bool PersonalSettings::write(const QString &path, const PersonalSettings &settings, QString *errorMessage)
{
    QByteArray existing;
    if (QFile file(path); file.open(QIODevice::ReadOnly))
        existing = file.readAll();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(settings.toYaml(existing)) < 0 || !file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

PersonalSettings PersonalSettings::fromYaml(const QByteArray &yaml)
{
    PersonalSettings settings;
    try {
        const YAML::Node root = YAML::Load(yaml.toStdString());
        if (!root.IsMap())
            return settings;
        settings.name = text(root["name"]);
        settings.birthYear = qMax(0, text(root["birthYear"]).toInt());
        settings.fideId = text(root["fideId"]);
        settings.boardTheme = text(root["boardTheme"]);
    } catch (const YAML::Exception &) {
    }
    return settings;
}

QByteArray PersonalSettings::toYaml(const QByteArray &existing) const
{
    YAML::Node root;
    try {
        root = YAML::Load(existing.toStdString());
    } catch (const YAML::Exception &) {
    }
    if (!root.IsMap())
        root = YAML::Node(YAML::NodeType::Map);
    const auto set = [&root](const char *key, const QString &value) {
        if (value.isEmpty())
            root.remove(key);
        else
            root[key] = value.toStdString();
    };
    set("name", name.trimmed());
    if (birthYear > 0)
        root["birthYear"] = birthYear;
    else
        root.remove("birthYear");
    set("fideId", fideId.trimmed());
    set("boardTheme", boardTheme.trimmed());
    root.remove("appearance"); // Kept here by a development version: it is per computer now.

    YAML::Emitter out;
    out << YAML::Comment("Pragma Chess: who you are, the same on every synced computer") << YAML::Newline;
    out << root;
    return QByteArray(out.c_str()) + '\n';
}

QString PersonalSettings::generatedName(quint32 random)
{
    const int champions = RoomName::championCount();
    QString champion = RoomName::champion(int(random % quint32(champions)));
    champion.remove(QLatin1Char(' ')); // "La Bourdonnais" → "LaBourdonnais007"
    return champion + QStringLiteral("%1").arg((random / quint32(champions)) % 1000, 3, 10, QLatin1Char('0'));
}

QString PersonalSettings::nameIn(const PlayerRoles &roles) const
{
    // The first "me" in the order of the names, so the choice does not depend on the hash.
    QString me;
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
        if (it.value() == PlayerRole::Me && (me.isEmpty() || it.key() < me))
            me = it.key();
    }
    return me.isEmpty() ? name.trimmed() : me;
}
