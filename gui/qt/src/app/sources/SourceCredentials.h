#pragma once

#include <QString>

/// Access tokens of sources, kept per user and never in the database file,
/// which may be copied or shared. Keyed by GameSource::uuid.
///
/// Interim storage in the user's settings; to move to the system keychain.
namespace SourceCredentials {

QString token(const QString &sourceUuid);
void setToken(const QString &sourceUuid, const QString &token);
void remove(const QString &sourceUuid);

} // namespace SourceCredentials
