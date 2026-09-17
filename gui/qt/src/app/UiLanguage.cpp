#include "UiLanguage.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {

const QString kSettingKey = QStringLiteral("ui/language");

} // namespace

namespace UiLanguage {

QList<Language> available()
{
    return {
        {QString(), QCoreApplication::translate("UiLanguage", "System Language")},
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("it"), QStringLiteral("Italiano")},
    };
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
    const QString system = QLocale(languages.isEmpty() ? QLocale::system().name() : languages.first())
                               .name()
                               .section(QLatin1Char('_'), 0, 0);
    for (const Language &language : available()) {
        if (language.code == system)
            return system;
    }
    return QStringLiteral("en");
}

void install(QCoreApplication &app)
{
    const QString code = effective();
    if (code == QLatin1String("en"))
        return;
    auto *qt = new QTranslator(&app);
    if (qt->load(QStringLiteral("qtbase_") + code, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(qt);
    auto *own = new QTranslator(&app);
    if (own->load(QStringLiteral(":/i18n/pragma-chess_") + code))
        QCoreApplication::installTranslator(own);
    // Dates and numbers follow the interface language when it was chosen.
    if (!chosen().isEmpty())
        QLocale::setDefault(QLocale(code));
}

} // namespace UiLanguage
