#include "OpeningNames.h"

#include "Pgn.h"
#include "PolyglotBook.h"

OpeningNames::OpeningNames(const QList<GameRecord> &games)
{
    // Named lines share their first moves: replay each prefix only once.
    QHash<QString, ChessPosition> replayed;
    for (const GameRecord &game : games) {
        if (game.event.trimmed().isEmpty())
            continue;
        std::optional<ChessPosition> position =
            game.startFen.isEmpty() ? ChessPosition::startingPosition() : ChessPosition::fromFen(game.startFen);
        if (!position)
            continue;
        QString prefix = game.startFen;
        int plies = 0;
        for (const MoveRecord &record : game.moves) {
            prefix += QLatin1Char(' ') + record.uci;
            if (const auto known = replayed.constFind(prefix); known != replayed.cend()) {
                *position = *known;
            } else {
                const std::optional<ChessMove> move = position->moveFromUci(record.uci);
                if (!move)
                    break;
                position->play(*move);
                replayed.insert(prefix, *position);
            }
            ++plies;
        }
        if (plies != game.moves.size())
            continue; // A line that does not replay names nothing.
        const quint64 key = PolyglotBook::key(*position);
        const auto existing = m_names.constFind(key);
        if (existing == m_names.cend() || plies < existing->plies)
            m_names.insert(key, Entry{Name{game.eco, game.event.trimmed()}, plies});
    }
}

OpeningNames::Name OpeningNames::name(const ChessPosition &position) const
{
    return m_names.value(PolyglotBook::key(position)).name;
}

QList<GameRecord> OpeningNames::gamesFromTsv(const QString &tsv)
{
    QList<GameRecord> games;
    const QStringList lines = tsv.split(QLatin1Char('\n'));
    if (lines.isEmpty())
        return games;
    const QStringList header = lines.first().trimmed().split(QLatin1Char('\t'));
    const qsizetype ecoColumn = header.indexOf(QStringLiteral("eco"));
    const qsizetype nameColumn = header.indexOf(QStringLiteral("name"));
    const qsizetype pgnColumn = header.indexOf(QStringLiteral("pgn"));
    if (ecoColumn < 0 || nameColumn < 0 || pgnColumn < 0)
        return games;

    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QStringList fields = lines.at(i).trimmed().split(QLatin1Char('\t'));
        if (fields.size() < header.size())
            continue;
        QString error;
        const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(fields.at(pgnColumn), QString(), &error);
        if (!line)
            continue;
        GameRecord game;
        game.eco = fields.at(ecoColumn);
        game.event = fields.at(nameColumn);
        game.result = QStringLiteral("*");
        game.startFen = line->startFen;
        game.moves = line->moves;
        game.plyCount = int(line->moves.size());
        games << game;
    }
    return games;
}
