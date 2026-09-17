#include "UserFolders.h"

#include <QDir>
#include <QHash>
#include <QStandardPaths>

namespace {

const QHash<QLocale::Language, QString> &localizedNames()
{
    static const QHash<QLocale::Language, QString> names{
        {QLocale::English, QStringLiteral("Chess")},
        {QLocale::Italian, QStringLiteral("Scacchi")},
        {QLocale::German, QStringLiteral("Schach")},
        {QLocale::French, QStringLiteral("Échecs")},
        {QLocale::Spanish, QStringLiteral("Ajedrez")},
        {QLocale::Catalan, QStringLiteral("Escacs")},
        {QLocale::Portuguese, QStringLiteral("Xadrez")},
        {QLocale::Dutch, QStringLiteral("Schaken")},
        {QLocale::Swedish, QStringLiteral("Schack")},
        {QLocale::Danish, QStringLiteral("Skak")},
        {QLocale::NorwegianBokmal, QStringLiteral("Sjakk")},
        {QLocale::Finnish, QStringLiteral("Shakki")},
        {QLocale::Polish, QStringLiteral("Szachy")},
        {QLocale::Czech, QStringLiteral("Šachy")},
        {QLocale::Slovak, QStringLiteral("Šach")},
        {QLocale::Croatian, QStringLiteral("Šah")},
        {QLocale::Slovenian, QStringLiteral("Šah")},
        {QLocale::Hungarian, QStringLiteral("Sakk")},
        {QLocale::Romanian, QStringLiteral("Șah")},
        {QLocale::Turkish, QStringLiteral("Satranç")},
        {QLocale::Greek, QStringLiteral("Σκάκι")},
        {QLocale::Russian, QStringLiteral("Шахматы")},
        {QLocale::Ukrainian, QStringLiteral("Шахи")},
        {QLocale::Chinese, QStringLiteral("国际象棋")},
        {QLocale::Japanese, QStringLiteral("チェス")},
        {QLocale::Korean, QStringLiteral("체스")},
    };
    return names;
}

QLocale uiLocale()
{
    // uiLanguages() honors LANGUAGE / LC_MESSAGES, like the XDG user folders do.
    const QStringList languages = QLocale::system().uiLanguages();
    return languages.isEmpty() ? QLocale::system() : QLocale(languages.first());
}

} // namespace

namespace UserFolders {

QString chessFolderName(const QLocale &locale)
{
    return localizedNames().value(locale.language(), QStringLiteral("Chess"));
}

QString chessDir()
{
    const QString override = qEnvironmentVariable("PRAGMA_CHESS_DIR");
    if (!override.isEmpty())
        return QDir::cleanPath(override);

    const QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    const QString preferred = chessFolderName(uiLocale());
    if (home.exists(preferred + QStringLiteral("/Pragma")))
        return home.filePath(preferred);

    // Reuse a folder created under a different language.
    for (const QString &name : localizedNames()) {
        if (home.exists(name + QStringLiteral("/Pragma")))
            return home.filePath(name);
    }
    return home.filePath(preferred);
}

QString pragmaDir()
{
    return QDir(chessDir()).filePath(QStringLiteral("Pragma"));
}

QString databasesDir()
{
    return QDir(pragmaDir()).filePath(QStringLiteral("Databases"));
}

QString projectsDir()
{
    return QDir(pragmaDir()).filePath(QStringLiteral("Projects"));
}

QString booksDir()
{
    return QDir(pragmaDir()).filePath(QStringLiteral("Books"));
}

bool ensureBooksDir()
{
    return QDir().mkpath(booksDir());
}

bool ensureProjectsDir()
{
    return QDir().mkpath(projectsDir());
}

bool ensureDatabasesDir()
{
    return QDir().mkpath(databasesDir());
}

} // namespace UserFolders
