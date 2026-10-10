#pragma once

#include <QByteArray>
#include <QList>
#include <QRegularExpression>
#include <QString>

/// The `.pragmaignore` of the remote folder: files the server keeps to
/// itself. They are never downloaded to a device nor uploaded from one (a
/// README of the repository, its LICENSE…), and a device that received one
/// before, unchanged since, lets it go.
///
/// One pattern a line, as `.gitignore` writes them: `#` starts a comment,
/// `*` and `?` stay within a name, `**` crosses folders, a pattern without
/// `/` matches a name at any depth, a leading `/` (or one inside) anchors it
/// to the root, a trailing `/` matches folders only, and `!` takes a path
/// back; the last pattern that matches decides. Matching a folder matches
/// everything in it.
class SyncIgnore {
public:
    static constexpr char fileName[] = ".pragmaignore";

    static SyncIgnore parse(const QByteArray &text);

    /// Whether the file at `path` (relative, '/' separators) stays on the server.
    bool matches(const QString &path) const;
    bool isEmpty() const { return m_rules.isEmpty(); }

private:
    struct Rule {
        QRegularExpression pattern;
        bool negated = false;
        bool foldersOnly = false;
        /// Against the whole path from the root; otherwise against each name.
        bool anchored = false;
    };
    QList<Rule> m_rules;
};
