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

/// The sites of the collections En Croissant converts, where their authors
/// say on what terms (tech/downloads.md): its catalog names none.
QString homepageOf(const QString &title)
{
    static const QList<std::pair<QString, QString>> sites{
        {QStringLiteral("Lumbra"), QStringLiteral("https://lumbrasgigabase.com/en/")},
        {QStringLiteral("Caissabase"), QStringLiteral("http://caissabase.co.uk/")},
        {QStringLiteral("Ajedrez Data"), QStringLiteral("https://www.ajedrezdata.com/")},
        {QStringLiteral("Lichess"), QStringLiteral("https://database.lichess.org/")},
    };
    for (const auto &[name, site] : sites) {
        if (title.startsWith(name, Qt::CaseInsensitive))
            return site;
    }
    return {};
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

QList<Extension> databases(const QByteArray &json, bool puzzles)
{
    QList<Extension> found;
    for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject entry = value.toObject();
        Extension database;
        database.kind = Extension::Kind::Database;
        database.puzzles = puzzles;
        database.name = entry.value(QStringLiteral("title")).toString();
        database.id = slug(database.name);
        database.description = entry.value(QStringLiteral("description")).toString();
        database.downloadUrl = entry.value(QStringLiteral("downloadLink")).toString();
        database.downloadSize = entry.value(QStringLiteral("storage_size")).toInteger();
        database.count = entry.value(puzzles ? QStringLiteral("puzzle_count")
                                                                     : QStringLiteral("game_count")).toInteger();
        database.author = QUrl(database.downloadUrl).host();
        database.homepage = homepageOf(database.name);
        // Its games are converted on this computer (EnCroissantConversion); its
        // puzzles are in another format of En Croissant's, not read yet.
        if (puzzles)
            database.unavailable = Text::tr("In a format of En Croissant's that Pragma Chess does not read yet.");
        if (!database.name.isEmpty())
            found << database;
    }
    return found;
}

} // namespace EnCroissantCatalog
