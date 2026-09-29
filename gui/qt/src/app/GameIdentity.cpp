#include "GameIdentity.h"

#include <QDateTime>
#include <QStringList>

namespace GameIdentity {

QString content(const GameRecord &game)
{
    QStringList uci;
    for (const MoveRecord &move : game.moves)
        uci << move.uci.trimmed();
    return QStringList{game.white.trimmed(),    game.black.trimmed(), game.event.trimmed(),
                       game.site.trimmed(),     game.date.trimmed(),  game.round.trimmed(),
                       game.result.trimmed(),   game.startFen.trimmed(), uci.join(QLatin1Char(' '))}
        .join(QLatin1Char('\n'));
}

QString uid(const GameRecord &game, int occurrence)
{
    QString name = content(game);
    if (occurrence > 1)
        name += QStringLiteral("\n#%1").arg(occurrence);
    return uuidV5(kGameNamespace, name.toUtf8());
}

QString uuidV5(const QUuid &nameSpace, const QByteArray &name)
{
    return QUuid::createUuidV5(nameSpace, name).toString(QUuid::WithoutBraces);
}

QString newLineageId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString now()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

bool sameContent(const GameRecord &a, const GameRecord &b)
{
    return content(a) == content(b) && a.eco.trimmed() == b.eco.trimmed() && a.whiteElo == b.whiteElo
           && a.blackElo == b.blackElo;
}

} // namespace GameIdentity
