#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

/// A drawer of Edit ▸ Drawers…: a name and what it holds — moves, a
/// variation, a position, any text — to put things in and take them out
/// again by name.
struct Drawer {
    QString name;
    QString content;

    bool operator==(const Drawer &) const = default;
};

/// The user's drawers, the same on every synced computer: kept under
/// `drawers` in `.pragma-chess.conf` (PersonalSettings::path()), beside the
/// personal settings, each side keeping the keys of the other.
struct Drawers {
    QList<Drawer> list;

    bool operator==(const Drawers &) const = default;

    /// What `path` holds; no drawers when there is no file or it cannot be read.
    static Drawers read(const QString &path);
    /// Writes the drawers into `path`, keeping its other keys.
    static bool write(const QString &path, const Drawers &drawers, QString *errorMessage);

    static Drawers fromYaml(const QByteArray &yaml);
    /// `existing` is the file as it was: its other keys are kept.
    QByteArray toYaml(const QByteArray &existing = QByteArray()) const;

    /// What keeps the drawers from being saved, for the user; empty when
    /// nothing does: every drawer needs a name, and no two the same one
    /// (they are called by it), whatever the case.
    QString problem() const;
};
