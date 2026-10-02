#include "HelpGuide.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

/// Lower case and without accents, one character for each one of `text`, so
/// that positions found here are positions in `text`.
QString folded(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar c : text) {
        const QString base = c.decomposition().isEmpty() ? QString(c) : c.decomposition();
        result += base.at(0).toLower();
    }
    return result;
}

/// The words of a topic without the marks of Markdown.
QString plainText(const QString &markdown)
{
    QString text = markdown;
    static const QRegularExpression link(QStringLiteral(R"(\[([^\]]*)\]\([^)]*\))"));
    text.replace(link, QStringLiteral("\\1"));
    // Emphasis goes without a trace ("**Trash**," reads "Trash,"), the rest leaves a space.
    static const QRegularExpression emphasis(QStringLiteral(R"([*`]+)"));
    text.remove(emphasis);
    static const QRegularExpression marks(QStringLiteral(R"([#|>]+)"));
    text.replace(marks, QStringLiteral(" "));
    static const QRegularExpression bullets(QStringLiteral(R"((^|\n)\s*-\s+)"));
    text.replace(bullets, QStringLiteral("\\1"));
    return text.simplified();
}

/// About `around` characters of `text` each side of the match, cut at words.
QString snippetAt(const QString &text, qsizetype position, qsizetype length, qsizetype around = 42)
{
    qsizetype from = qMax<qsizetype>(0, position - around);
    qsizetype to = qMin(text.size(), position + length + around);
    if (from > 0) {
        const qsizetype space = text.indexOf(QLatin1Char(' '), from);
        from = space >= 0 && space < position ? space + 1 : from;
    }
    if (to < text.size()) {
        const qsizetype space = text.lastIndexOf(QLatin1Char(' '), to);
        to = space > position + length ? space : to;
    }
    QString snippet = text.mid(from, to - from);
    if (from > 0)
        snippet.prepend(QChar(0x2026));
    if (to < text.size())
        snippet.append(QChar(0x2026));
    return snippet;
}

} // namespace

HelpGuide HelpGuide::fromMarkdown(const QString &markdown)
{
    HelpGuide guide;
    static const QRegularExpression heading(QStringLiteral(R"(^#\s+(.+?)\s*(?:\{#([A-Za-z0-9-]+)\})?\s*$)"));
    QStringList body;
    const auto close = [&] {
        if (guide.m_topics.isEmpty())
            return;
        Topic &topic = guide.m_topics.last();
        const QString text = body.join(QLatin1Char('\n')).trimmed();
        topic.markdown = QStringLiteral("# %1\n\n%2\n").arg(topic.title, text);
        topic.text = plainText(text);
    };
    for (const QString &line : markdown.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = heading.match(line);
        if (!match.hasMatch()) {
            body << line;
            continue;
        }
        close();
        body.clear();
        Topic topic;
        topic.title = match.captured(1);
        topic.id = match.captured(2).isEmpty() ? folded(topic.title).simplified().replace(QLatin1Char(' '), QLatin1Char('-'))
                                               : match.captured(2);
        guide.m_topics << topic;
    }
    close();
    return guide;
}

int HelpGuide::indexOf(const QString &id) const
{
    for (int index = 0; index < m_topics.size(); ++index) {
        if (m_topics.at(index).id == id)
            return index;
    }
    return -1;
}

QList<HelpGuide::Match> HelpGuide::search(const QString &query) const
{
    const QStringList words = folded(query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QList<Match> inTitle;
    QList<Match> inText;
    for (int index = 0; index < m_topics.size() && !words.isEmpty(); ++index) {
        const Topic &topic = m_topics.at(index);
        const QString title = folded(topic.title);
        const QString text = folded(topic.text);
        bool all = true;
        bool titled = false;
        qsizetype position = -1;
        qsizetype length = 0;
        for (const QString &word : words) {
            const qsizetype found = text.indexOf(word);
            const bool here = title.contains(word);
            all = all && (found >= 0 || here);
            titled = titled || here;
            if (found >= 0 && (position < 0 || found < position)) {
                position = found;
                length = word.size();
            }
        }
        if (!all)
            continue;
        // A match in the title alone is told by how the topic begins.
        const Match match{index, position >= 0 ? snippetAt(topic.text, position, length)
                                               : snippetAt(topic.text, 0, 0, 84)};
        (titled ? inTitle : inText) << match;
    }
    return inTitle + inText;
}
