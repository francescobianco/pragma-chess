#include "GameVariations.h"

#include "ChessPosition.h"
#include "MoveAnnotation.h"

#include <QStringList>

namespace GameVariations {

namespace {

void write(const QList<Variation> &variations, QStringList &words)
{
    for (const Variation &variation : variations) {
        words << QStringLiteral("(%1").arg(variation.atPly);
        for (const MoveRecord &move : variation.moves)
            words << move.san + MoveAnnotation::storedSuffix(move.nags);
        write(variation.variations, words);
        words << QStringLiteral(")");
    }
}

/// Reads one variation starting at `i` (just after its "("), to its ")".
Variation read(const QStringList &words, qsizetype &i)
{
    Variation variation;
    while (i < words.size()) {
        const QString &word = words.at(i++);
        if (word == QLatin1String(")"))
            break;
        if (word.startsWith(QLatin1Char('('))) {
            Variation inner = read(words, i);
            inner.atPly = word.mid(1).toInt();
            variation.variations << inner;
            continue;
        }
        MoveRecord move;
        move.san = MoveAnnotation::split(word, &move.nags);
        variation.moves << move;
    }
    return variation;
}

/// Replays `moves` from `position`, filling in what is missing and cutting
/// at the first illegal move; then does the same for the variations off it.
void resolveLine(QList<MoveRecord> &moves, QList<Variation> &variations, const ChessPosition &start, bool cutMain)
{
    QList<ChessPosition> positions{start};
    for (qsizetype i = 0; i < moves.size(); ++i) {
        MoveRecord &move = moves[i];
        const ChessPosition &current = positions.last();
        std::optional<ChessMove> played = move.uci.isEmpty() ? std::nullopt : current.moveFromUci(move.uci);
        if (!played && !move.san.isEmpty())
            played = current.moveFromSan(move.san);
        if (!played) {
            if (cutMain)
                moves.resize(i);
            break;
        }
        move.uci = played->uci();
        move.san = current.san(*played);
        ChessPosition next = current;
        next.play(*played);
        positions << next;
    }
    for (qsizetype v = 0; v < variations.size();) {
        Variation &variation = variations[v];
        // The position before the move it is an alternative to.
        if (variation.atPly < 1 || variation.atPly > positions.size() - 1) {
            variations.removeAt(v);
            continue;
        }
        resolveLine(variation.moves, variation.variations, positions.at(variation.atPly - 1), true);
        if (variation.moves.isEmpty()) {
            variations.removeAt(v);
            continue;
        }
        ++v;
    }
}

} // namespace

QString toText(const QList<Variation> &variations)
{
    QStringList words;
    write(variations, words);
    return words.isEmpty() ? QStringLiteral("") : words.join(QLatin1Char(' ')); // Empty, never null: the column is NOT NULL.
}

QList<Variation> fromText(const QString &text)
{
    QList<Variation> variations;
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < words.size();) {
        const QString &word = words.at(i++);
        if (!word.startsWith(QLatin1Char('(')))
            continue; // Not a variation: nothing else belongs at this level.
        Variation variation = read(words, i);
        variation.atPly = word.mid(1).toInt();
        variations << variation;
    }
    return variations;
}

void resolve(GameRecord &game, const ChessPosition &start)
{
    resolveLine(game.moves, game.variations, start, false);
}

const QList<Variation> *variationsOf(const GameRecord &game, const QList<int> &path)
{
    const QList<Variation> *variations = &game.variations;
    for (const int index : path) {
        if (index < 0 || index >= variations->size())
            return nullptr;
        variations = &variations->at(index).variations;
    }
    return variations;
}

QList<Variation> *variationsOf(GameRecord &game, const QList<int> &path)
{
    return const_cast<QList<Variation> *>(variationsOf(std::as_const(game), path));
}

QList<MoveRecord> lineMoves(const GameRecord &game, const QList<int> &path)
{
    QList<MoveRecord> line = game.moves;
    const QList<Variation> *variations = &game.variations;
    int base = 0; // Where the moves of the line taken begin, in `line`.
    for (const int index : path) {
        if (index < 0 || index >= variations->size())
            return game.moves;
        const Variation &variation = variations->at(index);
        const int branch = base + variation.atPly - 1;
        if (branch < 0 || branch > line.size())
            return game.moves;
        line = line.first(branch) + variation.moves;
        base = branch;
        variations = &variation.variations;
    }
    return line;
}

int branchPly(const GameRecord &game, const QList<int> &path)
{
    int ply = 0;
    int base = 0;
    const QList<Variation> *variations = &game.variations;
    for (const int index : path) {
        if (index < 0 || index >= variations->size())
            return 0;
        const Variation &variation = variations->at(index);
        ply = base + variation.atPly - 1;
        base = ply;
        variations = &variation.variations;
    }
    return ply;
}

} // namespace GameVariations
