#pragma once

#include "app/phone/NostrKey.h"

#include <QString>

/// The key the user signs their lobby events with — who they are in the
/// lobby. Kept on this computer only (QSettings `lobby/key`), never in the
/// synced Pragma folder: a secret key copied around by the sync could be
/// read wherever the folder goes. The user carries it to another computer
/// by hand, in Options ▸ Personal Settings…, to be themselves there too.
namespace LobbyIdentity {

/// This computer's key; made the first time it is asked for.
NostrKey key();
/// The secret key as the user copies it: 64 hex digits.
QString secretText();
/// Takes the key the user brought from another computer; false when the
/// text is not a valid secret key.
bool setSecretText(const QString &text);
/// Whether `text` is a secret key setSecretText would take.
bool isValidSecretText(const QString &text);

} // namespace LobbyIdentity
