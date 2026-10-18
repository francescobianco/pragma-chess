#include "UiLanguage.h"

#include "app/LocalizedText.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#include <algorithm>

namespace {

const QString kSettingKey = QStringLiteral("ui/language");

} // namespace

namespace UiLanguage {

QList<Language> available()
{
    // English, and every language a translation was built into the
    // application for (translations/pragma-chess_<code>.ts): a new language
    // needs its file only.
    QList<Language> languages{{QStringLiteral("en"), LocalizedText::nativeName(QStringLiteral("en"))}};
    const QStringList files = QDir(QStringLiteral(":/i18n"))
                                  .entryList({QStringLiteral("pragma-chess_*.qm")}, QDir::Files);
    for (const QString &file : files) {
        const QString code = file.mid(13).chopped(3);
        languages.append({code, LocalizedText::nativeName(code)});
    }
    std::sort(languages.begin(), languages.end(), [](const Language &a, const Language &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    languages.prepend({QString(), QCoreApplication::translate("UiLanguage", "System Language")});
    return languages;
}

QString chosen()
{
    return QSettings().value(kSettingKey).toString();
}

void setChosen(const QString &code)
{
    QSettings settings;
    if (code.isEmpty())
        settings.remove(kSettingKey);
    else
        settings.setValue(kSettingKey, code);
}

QString effective()
{
    const QString code = chosen();
    if (!code.isEmpty())
        return code;
    // uiLanguages() honors LANGUAGE / LC_MESSAGES.
    const QStringList languages = QLocale::system().uiLanguages();
    const QLocale system(languages.isEmpty() ? QLocale::system().name() : languages.first());
    // A translation for the very country first (zh_CN), else one of the
    // language in the same script: zh_CN serves Singapore, not Taiwan.
    const QList<Language> translations = available();
    for (const Language &language : translations) {
        if (language.code == system.name())
            return language.code;
    }
    for (const Language &language : translations) {
        const QLocale locale(language.code);
        if (!language.code.isEmpty() && locale.language() == system.language()
            && locale.script() == system.script())
            return language.code;
    }
    return QStringLiteral("en");
}

void install(QCoreApplication &app)
{
    const QString code = effective();
    if (code == QLatin1String("en"))
        return;
    // Qt's own texts: those of the Qt in use when it has them (windeployqt
    // names the catalog qt_<code>), else the copy embedded with ours.
    auto *qt = new QTranslator(&app);
    const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qt->load(QStringLiteral("qtbase_") + code, qtDir) || qt->load(QStringLiteral("qt_") + code, qtDir)
        || qt->load(QStringLiteral(":/i18n/qtbase_") + code))
        QCoreApplication::installTranslator(qt);
    auto *own = new QTranslator(&app);
    if (own->load(QStringLiteral(":/i18n/pragma-chess_") + code))
        QCoreApplication::installTranslator(own);
    // Dates and numbers follow the interface language when it was chosen.
    if (!chosen().isEmpty())
        QLocale::setDefault(QLocale(code));
}

} // namespace UiLanguage
