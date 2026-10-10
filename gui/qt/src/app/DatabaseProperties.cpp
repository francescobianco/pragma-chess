#include "DatabaseProperties.h"

#include <QFileInfo>

namespace {

const QString kIdKey = QStringLiteral("id");
const QString kTypeKey = QStringLiteral("type");
const QString kDescriptionKey = QStringLiteral("description");
const QString kNameKey = QStringLiteral("name");
const QString kNamePrefix = QStringLiteral("name.");
const QString kHiddenColumnsKey = QStringLiteral("columns.hidden");
const QString kShippedColumnsKey = QStringLiteral("columns.shipped");
const QString kColumnNamePrefix = QStringLiteral("columns.name.");

} // namespace

DatabaseProperties DatabaseProperties::fromValues(const QHash<QString, QString> &values)
{
    DatabaseProperties properties;
    properties.id = values.value(kIdKey);
    properties.type = typeFromKey(values.value(kTypeKey));
    properties.description = values.value(kDescriptionKey);
    properties.name = values.value(kNameKey);
    properties.hiddenColumns = values.value(kHiddenColumnsKey).split(QLatin1Char(','), Qt::SkipEmptyParts);
    const QString shipped = values.value(kShippedColumnsKey);
    // "1": the first mark, when the defaults were these five.
    properties.shippedColumns = shipped == QLatin1String("1")
        ? QStringList{QStringLiteral("white-elo"), QStringLiteral("black-elo"), QStringLiteral("result"),
                      QStringLiteral("date"), QStringLiteral("site")}
        : shipped.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        // "columns.name.event.it"
        if (it.key().startsWith(kColumnNamePrefix) && !it.value().isEmpty()) {
            const QString rest = it.key().mid(kColumnNamePrefix.size());
            const qsizetype dot = rest.lastIndexOf(QLatin1Char('.'));
            if (dot > 0)
                properties.columnNames[rest.left(dot)].insert(rest.mid(dot + 1).toLower(), it.value());
        }
        if (it.key().startsWith(kNamePrefix) && it.key().size() > kNamePrefix.size() && !it.value().isEmpty())
            properties.localizedNames.insert(it.key().mid(kNamePrefix.size()).toLower(), it.value());
    }
    // Written before the two carriers were told apart: a distributed
    // database's `name` was its English name.
    if (properties.isDistributed() && !properties.name.isEmpty()
        && !properties.localizedNames.contains(QStringLiteral("en"))) {
        properties.localizedNames.insert(QStringLiteral("en"), properties.name);
        properties.name.clear();
    }
    return properties;
}

QHash<QString, QString> DatabaseProperties::values() const
{
    QHash<QString, QString> result{{kTypeKey, typeKey(type)},
                                   {kDescriptionKey, description},
                                   {kHiddenColumnsKey, hiddenColumns.join(QLatin1Char(','))}};
    if (!id.isEmpty())
        result.insert(kIdKey, id);
    for (auto column = columnNames.cbegin(); column != columnNames.cend(); ++column)
        for (auto name = column.value().cbegin(); name != column.value().cend(); ++name)
            if (!name.value().isEmpty())
                result.insert(kColumnNamePrefix + column.key() + QLatin1Char('.') + name.key(), name.value());
    if (!shippedColumns.isEmpty())
        result.insert(kShippedColumnsKey, shippedColumns.join(QLatin1Char(',')));
    if (!name.isEmpty())
        result.insert(kNameKey, name);
    for (auto it = localizedNames.cbegin(); it != localizedNames.cend(); ++it) {
        if (!it.value().isEmpty())
            result.insert(kNamePrefix + it.key(), it.value());
    }
    return result;
}

QString DatabaseProperties::columnName(const QString &key, const QString &languageCode) const
{
    const QHash<QString, QString> names = columnNames.value(key);
    const QString code = languageCode.toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
    for (const QString &candidate : {code, code.section(QLatin1Char('_'), 0, 0), QStringLiteral("en")}) {
        if (const QString name = names.value(candidate); !name.isEmpty())
            return name;
    }
    return {};
}

QString DatabaseProperties::displayName(const QString &languageCode, const QString &fileBaseName) const
{
    if (!name.isEmpty())
        return name;
    if (isDistributed()) {
        const QString code = languageCode.toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
        if (const QString exact = localizedNames.value(code); !exact.isEmpty())
            return exact;
        if (const QString language = localizedNames.value(code.section(QLatin1Char('_'), 0, 0)); !language.isEmpty())
            return language;
        if (const QString english = localizedNames.value(QStringLiteral("en")); !english.isEmpty())
            return english;
        return localizedNames.cbegin().value();
    }
    return fileBaseName;
}

QString DatabaseProperties::givenName(const QString &languageCode, const QString &fileBaseName) const
{
    const QString shown = displayName(languageCode, fileBaseName).trimmed();
    return shown == fileBaseName ? QString() : shown;
}

QString DatabaseProperties::label(const QString &languageCode, const QString &path) const
{
    const QFileInfo file(path);
    // A distributed database not renamed is known by its name alone, in the user's language.
    if (isDistributed() && name.isEmpty())
        return displayName(languageCode, file.completeBaseName());
    const QString given = givenName(languageCode, file.completeBaseName());
    return given.isEmpty() ? file.completeBaseName() : QStringLiteral("%1 (%2)").arg(given, file.fileName());
}

QString DatabaseProperties::typeKey(DatabaseType type)
{
    switch (type) {
    case DatabaseType::OpeningBook: return QStringLiteral("opening-book");
    case DatabaseType::Training: return QStringLiteral("training");
    case DatabaseType::GameCollection: break;
    }
    return QStringLiteral("games");
}

DatabaseType DatabaseProperties::typeFromKey(const QString &key)
{
    if (key == QLatin1String("opening-book"))
        return DatabaseType::OpeningBook;
    if (key == QLatin1String("training"))
        return DatabaseType::Training;
    return DatabaseType::GameCollection;
}
