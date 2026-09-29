#include "FigurineFont.h"

#include <QFontDatabase>
#include <QStringList>

namespace FigurineFont {

namespace {

// Registered on first use, from the resource built by scripts/make-figurine-font.py.
QString family()
{
    static const QString registered = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/pragma-figurine.otf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QString() : families.first();
    }();
    return registered;
}

} // namespace

QFont apply(const QFont &base)
{
    const QString figurines = family();
    if (figurines.isEmpty())
        return base;
    QFont font = base;
    QStringList families = base.families();
    if (families.isEmpty())
        families.append(base.family());
    families.prepend(figurines);
    font.setFamilies(families);
    return font;
}

} // namespace FigurineFont
