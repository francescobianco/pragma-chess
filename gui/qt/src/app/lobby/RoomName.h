#pragma once

#include <QSet>
#include <QString>

/// The name of a tournament room, from a seed: a chess term and a champion,
/// "Capablanca's Fortress", "La fortezza di Capablanca". The seed is what
/// travels (a room is named by it on every device); each client shows it in
/// its own language, since a term is a whole phrase translated with its
/// article and its "of" ("%1's Gambit" → "Il gambetto di %1"), and the
/// champions' names are the same everywhere.
///
/// The lists only grow: a seed must name the same room tomorrow, so terms
/// and champions are appended, never removed or reordered.
namespace RoomName {

/// The name of `seed` in the interface language.
QString text(quint32 seed);
/// The same in English, untranslated.
QString englishText(quint32 seed);
/// The term and the champion `seed` picks.
int termOf(quint32 seed);
int championOf(quint32 seed);
int termCount();
int championCount();
/// The champion at `index` (0 to championCount() - 1), as the West writes the name.
QString champion(int index);
/// A new seed, drawn at random, whose name is none of `taken`'s.
quint32 newSeed(const QSet<quint32> &taken = {});

} // namespace RoomName
