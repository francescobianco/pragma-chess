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
        std::optional<ChessMove> played = move.uci.isEmpty() ? std::nullopt : current.moveFromUci(move.uci, ChessPosition::NullMoves::Allowed);
        if (!played && !move.san.isEmpty())
            played = current.moveFromSan(move.san, ChessPosition::NullMoves::Allowed);
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

namespace {

/// The moves and the variations of the line `path` leads to, to change them;
/// nothing for an invalid path.
struct Line {
    QList<MoveRecord> *moves = nullptr;
    QList<Variation> *variations = nullptr;
};

Line lineAt(GameRecord &game, const QList<int> &path)
{
    Line line{&game.moves, &game.variations};
    for (const int index : path) {
        if (index < 0 || index >= line.variations->size())
            return {};
        Variation &variation = (*line.variations)[index];
        line = {&variation.moves, &variation.variations};
    }
    return line;
}

} // namespace

std::optional<Edit> promote(const GameRecord &game, const QList<int> &path, int ply)
{
    if (path.isEmpty())
        return std::nullopt;
    Edit edit{game, path.first(path.size() - 1), ply};
    const Line parent = lineAt(edit.game, edit.path);
    const int index = path.last();
    if (!parent.moves || index < 0 || index >= parent.variations->size())
        return std::nullopt;
    const Variation variation = parent.variations->at(index);
    const int at = variation.atPly; // The parent's move it is an alternative to (1-based).
    if (at < 1 || at > parent.moves->size())
        return std::nullopt;
    // The parent's moves from `at` on become a variation in the promoted one's
    // place, taking along the variations that hung off them (renumbered from it).
    Variation tail;
    tail.atPly = at;
    tail.moves = parent.moves->mid(at - 1);
    tail.startComment = variation.startComment;
    QList<Variation> kept;
    for (int i = 0; i < parent.variations->size(); ++i) {
        const Variation &other = parent.variations->at(i);
        if (i == index) {
            kept << tail; // Filled below with what hangs off it.
        } else if (other.atPly > at) {
            Variation moved = other;
            moved.atPly = other.atPly - at + 1;
            tail.variations << moved;
        } else {
            kept << other;
        }
    }
    // The tail is the entry at the promoted variation's old index among those kept.
    int tailIndex = 0;
    for (int i = 0; i < index; ++i) {
        if (parent.variations->at(i).atPly <= at)
            ++tailIndex;
    }
    kept[tailIndex].variations = tail.variations;
    // The promoted moves, and their own variations, renumbered from the parent's first move.
    *parent.moves = parent.moves->first(at - 1) + variation.moves;
    for (Variation own : variation.variations) {
        own.atPly += at - 1;
        kept << own;
    }
    *parent.variations = kept;
    return edit;
}

std::optional<Edit> removeVariation(const GameRecord &game, const QList<int> &path)
{
    if (path.isEmpty())
        return std::nullopt;
    Edit edit{game, path.first(path.size() - 1), branchPly(game, path)};
    const Line parent = lineAt(edit.game, edit.path);
    const int index = path.last();
    if (!parent.moves || index < 0 || index >= parent.variations->size())
        return std::nullopt;
    parent.variations->removeAt(index);
    return edit;
}

std::optional<Edit> truncate(const GameRecord &game, const QList<int> &path, int ply)
{
    const int first = ply - branchPly(game, path); // 1-based among the line's own moves.
    if (first <= 1 && !path.isEmpty())
        return removeVariation(game, path);
    Edit edit{game, path, ply - 1};
    const Line line = lineAt(edit.game, path);
    if (!line.moves || first < 1 || first > line.moves->size())
        return std::nullopt;
    line.moves->resize(first - 1);
    line.variations->removeIf([first](const Variation &variation) { return variation.atPly >= first; });
    if (path.isEmpty())
        edit.game.plyCount = int(edit.game.moves.size());
    return edit;
}

} // namespace GameVariations
