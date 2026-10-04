#include "UserFolders.h"

#include <QDir>
#include <QHash>
#include <QSettings>
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

QString chosenOr(const QString &chosen, const QString &fallback)
{
    return chosen.trimmed().isEmpty() ? fallback : QDir::cleanPath(chosen.trimmed());
}

/// The folders of this run: the choices as they were at the first call.
const UserFolders::Folders &active()
{
    static const UserFolders::Folders folders = UserFolders::resolve(
        UserFolders::isOverridden() ? UserFolders::FolderChoice{} : UserFolders::chosenFolders(),
        UserFolders::defaultPragmaDir());
    return folders;
}

} // namespace

namespace UserFolders {

Folders resolve(const FolderChoice &choice, const QString &defaultPragma)
{
    Folders folders;
    folders.pragma = chosenOr(choice.pragma, defaultPragma);
    const QDir pragma(folders.pragma);
    folders.databases = chosenOr(choice.databases, pragma.filePath(QStringLiteral("Databases")));
    folders.projects = chosenOr(choice.projects, pragma.filePath(QStringLiteral("Projects")));
    folders.books = chosenOr(choice.books, pragma.filePath(QStringLiteral("Books")));
    folders.openingNames = chosenOr(choice.openingNames, QDir(folders.books).filePath(QStringLiteral("Opening Names")));
    return folders;
}

QString defaultPragmaDir()
{
    return QDir(chessDir()).filePath(QStringLiteral("Pragma"));
}

bool isOverridden()
{
    return !qEnvironmentVariable("PRAGMA_CHESS_DIR").isEmpty();
}

FolderChoice chosenFolders()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("folders"));
    FolderChoice choice;
    choice.pragma = settings.value(QStringLiteral("pragma")).toString();
    choice.databases = settings.value(QStringLiteral("databases")).toString();
    choice.projects = settings.value(QStringLiteral("projects")).toString();
    choice.books = settings.value(QStringLiteral("books")).toString();
    choice.openingNames = settings.value(QStringLiteral("openingNames")).toString();
    return choice;
}

void setChosenFolders(const FolderChoice &choice)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("folders"));
    const auto store = [&settings](const char *key, const QString &value) {
        if (value.trimmed().isEmpty())
            settings.remove(QLatin1String(key));
        else
            settings.setValue(QLatin1String(key), QDir::cleanPath(value.trimmed()));
    };
    store("pragma", choice.pragma);
    store("databases", choice.databases);
    store("projects", choice.projects);
    store("books", choice.books);
    store("openingNames", choice.openingNames);
}


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
    return active().pragma;
}

QString databasesDir()
{
    return active().databases;
}

QString projectsDir()
{
    return active().projects;
}

QString booksDir()
{
    return active().books;
}

QString openingNamesDir()
{
    return active().openingNames;
}

bool ensureBooksDir()
{
    return QDir().mkpath(booksDir());
}

bool ensureOpeningNamesDir()
{
    return QDir().mkpath(openingNamesDir());
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
