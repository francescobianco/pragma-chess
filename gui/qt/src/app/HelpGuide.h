#pragma once

#include <QList>
#include <QString>

/// The user guide: topics written in Markdown, one file per language
/// (`resources/help/guide_<code>.md`). A topic starts at a first-level
/// heading with its id, `# Title {#id}`; the ids are the same in every
/// language, so a topic can be found again when the language changes.
class HelpGuide {
public:
    struct Topic {
        QString id;
        QString title;
        /// The topic as written, heading included (without the id).
        QString markdown;
        /// Its words without the Markdown, on one line: what is searched.
        QString text;
    };

    /// A topic found by search(), with the words around what matched.
    struct Match {
        int topic = -1;
        QString snippet;
    };

    static HelpGuide fromMarkdown(const QString &markdown);

    const QList<Topic> &topics() const { return m_topics; }
    /// Index of the topic with `id`, or -1.
    int indexOf(const QString &id) const;

    /// The topics that contain every word of `query`, whatever the case and
    /// the accents: those with a word in the title first, then in the order
    /// of the guide. An empty query matches nothing.
    QList<Match> search(const QString &query) const;

private:
    QList<Topic> m_topics;
};
