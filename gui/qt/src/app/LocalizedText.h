#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

/// A text of a project — its name, a chapter's title, a paragraph — in
/// several languages. The project's structure is one; only its words change
/// with the language: shown in the language asked for, and where that one has
/// none, in English, then in the first other language that has it.
/// Pure, unit-tested.
class LocalizedText {
public:
    /// The languages a project's texts are written in: English first, it is
    /// the fallback of every other.
    static const QStringList &languages();
    /// The language of `code` among them ("it_IT" is "it"), English for one
    /// not supported.
    static QString supported(const QString &code);
    /// The language's own name: "English", "Italiano".
    static QString nativeName(const QString &language);

    LocalizedText() = default;
    LocalizedText(const QString &language, const QString &text);

    /// The text in `language`, or the fallback's.
    QString text(const QString &language) const;
    /// The text in `language` alone, empty when it has none.
    QString exact(const QString &language) const { return m_texts.value(language); }
    bool has(const QString &language) const { return m_texts.contains(language); }
    /// Writes the text of `language`; an empty one takes that language away.
    void set(const QString &language, const QString &text);
    /// No text in any language.
    bool isEmpty() const { return m_texts.isEmpty(); }
    /// The texts by language, the languages in order.
    const QMap<QString, QString> &texts() const { return m_texts; }

    bool operator==(const LocalizedText &) const = default;

private:
    QMap<QString, QString> m_texts;
};
