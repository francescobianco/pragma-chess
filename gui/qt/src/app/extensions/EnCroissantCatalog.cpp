#include "EnCroissantCatalog.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace EnCroissantCatalog {

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(EnCroissantCatalog)
};

const QString kSite = QStringLiteral("https://encroissant.org");

QString slug(const QString &text)
{
    static const QRegularExpression other(QStringLiteral("[^a-z0-9]+"));
    return text.toLower().replace(other, QStringLiteral("-")).remove(QRegularExpression(QStringLiteral("^-|-$")));
}

} // namespace

QString enginesUrl(const QString &system, bool bmi2)
{
    return QStringLiteral("%1/engines?os=%2&bmi2=%3").arg(kSite, system, bmi2 ? QStringLiteral("true") : QStringLiteral("false"));
}

QString databasesUrl()
{
    return kSite + QStringLiteral("/databases");
}

QString puzzlesUrl()
{
    return kSite + QStringLiteral("/puzzle_databases");
}

QList<Extension> engines(const QByteArray &json, const QString &system, bool bmi2)
{
    QList<Extension> found;
    for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("os")).toString() != system || entry.value(QStringLiteral("bmi2")).toBool() != bmi2)
            continue;
        Extension engine;
        engine.kind = Extension::Kind::Engine;
        engine.name = entry.value(QStringLiteral("name")).toString();
        engine.version = entry.value(QStringLiteral("version")).toString();
        engine.id = slug(engine.name + QLatin1Char('-') + engine.version + QLatin1Char('-') + system
                         + (bmi2 ? QStringLiteral("-bmi2") : QString()));
        engine.downloadUrl = entry.value(QStringLiteral("downloadLink")).toString();
        engine.executable = entry.value(QStringLiteral("path")).toString();
        engine.downloadSize = entry.value(QStringLiteral("downloadSize")).toInteger();
        engine.elo = entry.value(QStringLiteral("elo")).toInt();
        // Downloaded from its authors, not from En Croissant: their terms.
        engine.author = QUrl(engine.downloadUrl).host();
        if (engine.name.isEmpty() || engine.downloadUrl.isEmpty() || engine.executable.isEmpty())
            continue;
        found << engine;
    }
    return found;
}

QList<Extension> databases(const QByteArray &json, Extension::Kind kind)
{
    QList<Extension> found;
    for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject entry = value.toObject();
        Extension database;
        database.kind = kind;
        database.name = entry.value(QStringLiteral("title")).toString();
        database.id = slug(database.name);
        database.description = entry.value(QStringLiteral("description")).toString();
        database.downloadUrl = entry.value(QStringLiteral("downloadLink")).toString();
        database.downloadSize = entry.value(QStringLiteral("storage_size")).toInteger();
        database.count = entry.value(kind == Extension::Kind::Puzzles ? QStringLiteral("puzzle_count")
                                                                     : QStringLiteral("game_count")).toInteger();
        database.author = QUrl(database.downloadUrl).host();
        database.unavailable = Text::tr("In En Croissant's own format, which Pragma Chess does not read yet; "
                                        "its license is to be checked before it is offered.");
        if (!database.name.isEmpty())
            found << database;
    }
    return found;
}

} // namespace EnCroissantCatalog
