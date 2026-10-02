#include "MoveAnnotation.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(MoveAnnotation)
};

/// The suffix PGN writes glued to the move, for the six glyphs that have one.
QString suffixOf(int nag)
{
    return nag >= 1 && nag <= 6 ? MoveAnnotation::glyph(nag)->symbol : QString();
}

} // namespace

namespace MoveAnnotation {

const QList<Glyph> &glyphs()
{
    static const QList<Glyph> all{
        {3, QStringLiteral("!!"), Kind::Move},
        {1, QStringLiteral("!"), Kind::Move},
        {5, QStringLiteral("!?"), Kind::Move},
        {6, QStringLiteral("?!"), Kind::Move},
        {2, QStringLiteral("?"), Kind::Move},
        {4, QStringLiteral("??"), Kind::Move},
        {7, QStringLiteral("□"), Kind::Move},
        {18, QStringLiteral("+−"), Kind::Position},
        {16, QStringLiteral("±"), Kind::Position},
        {14, QStringLiteral("⩲"), Kind::Position},
        {10, QStringLiteral("="), Kind::Position},
        {13, QStringLiteral("∞"), Kind::Position},
        {15, QStringLiteral("⩱"), Kind::Position},
        {17, QStringLiteral("∓"), Kind::Position},
        {19, QStringLiteral("−+"), Kind::Position},
    };
    return all;
}

const Glyph *glyph(int nag)
{
    for (const Glyph &candidate : glyphs()) {
        if (candidate.nag == nag)
            return &candidate;
    }
    return nullptr;
}

QString meaning(int nag)
{
    switch (nag) {
    case 1: return Text::tr("good move");
    case 2: return Text::tr("mistake");
    case 3: return Text::tr("brilliant move");
    case 4: return Text::tr("blunder");
    case 5: return Text::tr("interesting move");
    case 6: return Text::tr("dubious move");
    case 7: return Text::tr("only move");
    case 10: return Text::tr("equal position");
    case 13: return Text::tr("unclear position");
    case 14: return Text::tr("White is slightly better");
    case 15: return Text::tr("Black is slightly better");
    case 16: return Text::tr("White is better");
    case 17: return Text::tr("Black is better");
    case 18: return Text::tr("White is winning");
    case 19: return Text::tr("Black is winning");
    default: return {};
    }
}

QList<int> normalized(const QList<int> &nags)
{
    int move = 0;
    int position = 0;
    for (const int nag : nags) {
        if (const Glyph *found = glyph(nag))
            (found->kind == Kind::Move ? move : position) = nag;
    }
    QList<int> result;
    if (move)
        result << move;
    if (position)
        result << position;
    return result;
}

QList<int> toggled(const QList<int> &nags, int nag)
{
    QList<int> result = normalized(nags);
    if (result.removeAll(nag) == 0)
        result << nag; // normalized() drops the one it replaces.
    return normalized(result);
}

QString symbols(const QList<int> &nags)
{
    QString text;
    for (const int nag : normalized(nags)) {
        const Glyph *found = glyph(nag);
        text += found->kind == Kind::Position ? QLatin1Char(' ') + found->symbol : found->symbol;
    }
    return text;
}

QString pgnSuffix(const QList<int> &nags)
{
    QString text;
    for (const int nag : normalized(nags)) {
        const QString suffix = suffixOf(nag);
        text += suffix.isEmpty() ? QStringLiteral(" $%1").arg(nag) : suffix;
    }
    return text;
}

QString storedSuffix(const QList<int> &nags)
{
    QString text;
    for (const int nag : normalized(nags)) {
        const QString suffix = suffixOf(nag);
        text += suffix.isEmpty() ? QStringLiteral("$%1").arg(nag) : suffix;
    }
    return text;
}

QString split(const QString &token, QList<int> *nags)
{
    static const QRegularExpression annotated(QStringLiteral(R"(^(.*?)([!?]{1,2})?((?:\$\d+)*)$)"));
    const QRegularExpressionMatch match = annotated.match(token);
    if (!match.hasMatch())
        return token;
    QList<int> found = *nags;
    for (const Glyph &candidate : glyphs()) {
        if (!match.captured(2).isEmpty() && candidate.symbol == match.captured(2))
            found << candidate.nag;
    }
    for (const QString &number : match.captured(3).split(QLatin1Char('$'), Qt::SkipEmptyParts))
        found << number.toInt();
    *nags = normalized(found);
    return match.captured(1);
}

} // namespace MoveAnnotation
