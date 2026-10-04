#pragma once

#include "PlayerRole.h"

#include <QByteArray>
#include <QString>

/// Options ▸ Personal Settings…: who the user is, the same on every computer.
/// Kept in `.pragma-chess.conf` (YAML) in the root of the Pragma folder,
/// which the folder sync carries even though it is hidden.
struct PersonalSettings {
    /// The name put on the user's side of new games.
    QString name;
    /// 0 when not given.
    int birthYear = 0;
    QString fideId;

    bool operator==(const PersonalSettings &) const = default;

    /// The file in the Pragma folder of this run.
    static QString path();
    /// What `path` holds; empty settings when there is no file or it cannot be read.
    static PersonalSettings read(const QString &path);
    /// Writes the settings over `path`, keeping the keys this version does not know.
    static bool write(const QString &path, const PersonalSettings &settings, QString *errorMessage);

    static PersonalSettings fromYaml(const QByteArray &yaml);
    /// `existing` is the file as it was: its other keys are kept.
    QByteArray toYaml(const QByteArray &existing = QByteArray()) const;

    /// The user's name in a database: the player marked "me" there (Who Is
    /// This?) wins, then the personal name; empty when neither says.
    QString nameIn(const PlayerRoles &roles) const;
};
