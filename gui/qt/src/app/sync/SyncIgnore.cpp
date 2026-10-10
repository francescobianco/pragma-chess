#include "SyncIgnore.h"

#include <QStringList>

namespace {

/// A pattern of names and folders as an anchored regular expression.
QString regexOf(const QString &pattern)
{
    QString regex;
    for (qsizetype i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern.at(i);
        if (c == QLatin1Char('*')) {
            if (i + 1 < pattern.size() && pattern.at(i + 1) == QLatin1Char('*')) {
                ++i;
                if (i + 1 < pattern.size() && pattern.at(i + 1) == QLatin1Char('/')) {
                    ++i;
                    regex += QStringLiteral("(?:.*/)?"); // "**/": any folders, or none.
                } else {
                    regex += QStringLiteral(".*");
                }
            } else {
                regex += QStringLiteral("[^/]*");
            }
        } else if (c == QLatin1Char('?')) {
            regex += QStringLiteral("[^/]");
        } else if (c == QLatin1Char('[')) {
            const qsizetype end = pattern.indexOf(QLatin1Char(']'), i + 1);
            if (end < 0) {
                regex += QStringLiteral("\\[");
                continue;
            }
            QString set = pattern.mid(i + 1, end - i - 1);
            if (set.startsWith(QLatin1Char('!')))
                set[0] = QLatin1Char('^');
            regex += QLatin1Char('[') + set.replace(QLatin1Char('\\'), QStringLiteral("\\\\")) + QLatin1Char(']');
            i = end;
        } else if (c == QLatin1Char('\\') && i + 1 < pattern.size()) {
            regex += QRegularExpression::escape(pattern.at(++i));
        } else {
            regex += QRegularExpression::escape(c);
        }
    }
    return QRegularExpression::anchoredPattern(regex);
}

} // namespace

SyncIgnore SyncIgnore::parse(const QByteArray &text)
{
    SyncIgnore ignore;
    const QStringList lines = QString::fromUtf8(text).split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        // Trailing spaces go, unless escaped; leading ones belong to the name.
        while (line.endsWith(QLatin1Char(' ')) && !line.endsWith(QLatin1String("\\ ")))
            line.chop(1);
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        Rule rule;
        if (line.startsWith(QLatin1Char('!'))) {
            rule.negated = true;
            line.remove(0, 1);
        } else if (line.startsWith(QLatin1String("\\!")) || line.startsWith(QLatin1String("\\#"))) {
            line.remove(0, 1);
        }
        if (line.endsWith(QLatin1Char('/'))) {
            rule.foldersOnly = true;
            line.chop(1);
        }
        rule.anchored = line.contains(QLatin1Char('/'));
        if (line.startsWith(QLatin1Char('/')))
            line.remove(0, 1);
        if (line.isEmpty())
            continue;
        rule.pattern = QRegularExpression(regexOf(line));
        if (rule.pattern.isValid())
            ignore.m_rules.append(rule);
    }
    return ignore;
}

bool SyncIgnore::matches(const QString &path) const
{
    const QStringList names = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    bool ignored = false;
    for (const Rule &rule : m_rules) {
        // The path itself, then each folder it is in: a folder that matches takes what it holds.
        bool hit = false;
        for (qsizetype depth = names.size(); depth >= 1 && !hit; --depth) {
            const bool folder = depth < names.size();
            if (rule.foldersOnly && !folder)
                continue;
            const QString subject = rule.anchored ? names.first(depth).join(QLatin1Char('/')) : names.at(depth - 1);
            hit = rule.pattern.match(subject).hasMatch();
        }
        if (hit)
            ignored = !rule.negated;
    }
    return ignored;
}
