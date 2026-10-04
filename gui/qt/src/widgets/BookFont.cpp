#include "BookFont.h"

#include "FigurineFont.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QStringList>

namespace BookFont {

namespace {

QString family()
{
    static const QString registered = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/crimson-pro.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QString() : families.first();
    }();
    return registered;
}

} // namespace

QFont paragraph(const QFont &base)
{
    QFont font = base;
    QStringList families{family(), QStringLiteral("serif")};
    families.removeAll(QString());
    font.setFamilies(families);
    // Crimson Pro is small on its body: a third larger, it reads as a book does.
    constexpr qreal kScale = 1.35;
    if (base.pointSizeF() > 0)
        font.setPointSizeF(base.pointSizeF() * kScale);
    else
        font.setPixelSize(qRound(base.pixelSize() * kScale));
    font.setStyleHint(QFont::Serif);
    font.setWeight(QFont::ExtraLight); // The face shipped: light, as a printed page.
    return FigurineFont::apply(font);
}

qreal indent(const QFont &paragraphFont)
{
    return QFontMetricsF(paragraphFont).horizontalAdvance(QLatin1Char('M')) * 1.5;
}

} // namespace BookFont
