#include "LocalizedText.h"

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
    if (language == QLatin1String("it"))
        return QStringLiteral("Italiano");
    if (language == QLatin1String("en"))
        return QStringLiteral("English");
    return language;
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

void LocalizedText::set(const QString &language, const QString &text)
{
    if (text.isEmpty())
        m_texts.remove(language);
    else
        m_texts.insert(language, text);
}
