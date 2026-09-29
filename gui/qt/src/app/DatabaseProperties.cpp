#include "DatabaseProperties.h"

namespace {

const QString kTypeKey = QStringLiteral("type");
const QString kDescriptionKey = QStringLiteral("description");

} // namespace

DatabaseProperties DatabaseProperties::fromValues(const QHash<QString, QString> &values)
{
    DatabaseProperties properties;
    properties.type = typeFromKey(values.value(kTypeKey));
    properties.description = values.value(kDescriptionKey);
    return properties;
}

QHash<QString, QString> DatabaseProperties::values() const
{
    return {{kTypeKey, typeKey(type)}, {kDescriptionKey, description}};
}

QString DatabaseProperties::typeKey(DatabaseType type)
{
    switch (type) {
    case DatabaseType::OpeningBook: return QStringLiteral("opening-book");
    case DatabaseType::GameCollection: break;
    }
    return QStringLiteral("games");
}

DatabaseType DatabaseProperties::typeFromKey(const QString &key)
{
    return key == QLatin1String("opening-book") ? DatabaseType::OpeningBook : DatabaseType::GameCollection;
}
