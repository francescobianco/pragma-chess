#include "BoardTheme.h"

namespace {

QString s_current = QLatin1String(BoardTheme::kDefaultId);

} // namespace

const QList<BoardTheme> &BoardTheme::all()
{
    static const QList<BoardTheme> themes{
        // The browns of the classic wooden board, with the pieces of chess books.
        {QLatin1String(kDefaultId), QStringLiteral("Pragma Classic"), QColor(0xf0, 0xd9, 0xb5),
         QColor(0xb5, 0x88, 0x63), QStringLiteral("companion")},
        // lichess.org's green board (public/images/board/green.png) and its Alpha pieces.
        {QStringLiteral("lichess-alpha"), QStringLiteral("Lichess Alpha"), QColor(0xff, 0xff, 0xdd),
         QColor(0x86, 0xa6, 0x66), QStringLiteral("alpha")},
    };
    return themes;
}

const BoardTheme &BoardTheme::byId(const QString &id)
{
    for (const BoardTheme &theme : all()) {
        if (theme.id == id)
            return theme;
    }
    return all().first();
}

const BoardTheme &BoardTheme::current()
{
    return byId(s_current);
}

void BoardTheme::setCurrent(const QString &id)
{
    s_current = byId(id).id;
}
