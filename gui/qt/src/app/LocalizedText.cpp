#include "LocalizedText.h"

#include <QHash>
#include <QLocale>

const QStringList &LocalizedText::languages()
{
    static const QStringList list{QStringLiteral("en"), QStringLiteral("it")};
    return list;
}

QString LocalizedText::supported(const QString &code)
{
    const QString language = code.section(QLatin1Char('_'), 0, 0).section(QLatin1Char('-'), 0, 0).toLower();
    return languages().contains(language) ? language : languages().constFirst();
}

QString LocalizedText::nativeName(const QString &language)
{
    // The language's own name, and only the language: Qt's native names
    // carry a country for some ("español de España", "American English").
    static const QHash<QString, QString> names{
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("it"), QStringLiteral("Italiano")},
        {QStringLiteral("es"), QStringLiteral("Español")},
    };
    if (const auto found = names.constFind(language); found != names.cend())
        return *found;
    // A language not named here yet: its name in English, never a country.
    const QLocale::Language known = QLocale(language).language();
    return known == QLocale::C || known == QLocale::AnyLanguage ? language : QLocale::languageToString(known);
}

LocalizedText::LocalizedText(const QString &language, const QString &text)
{
    set(language, text);
}

QString LocalizedText::text(const QString &language) const
{
    if (const auto found = m_texts.constFind(language); found != m_texts.cend())
        return *found;
    for (const QString &fallback : languages()) {
        if (const auto found = m_texts.constFind(fallback); found != m_texts.cend())
            return *found;
    }
    return m_texts.isEmpty() ? QString() : m_texts.first();
}

void LocalizedText::relabel(const QString &from, const QString &to)
{
    if (from == to || !m_texts.contains(from))
        return;
    m_texts.insert(to, m_texts.take(from));
}

void LocalizedText::set(const QString &language, const QString &text)
{
    if (text.isEmpty())
        m_texts.remove(language);
    else
        m_texts.insert(language, text);
}
