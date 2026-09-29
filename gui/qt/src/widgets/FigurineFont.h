#pragma once

#include <QFont>

/// The figurines of SAN (♘f3) drawn as in chess books, with the SkakNew
/// figurines shipped as "Pragma Figurine", which has only the five Unicode
/// piece symbols: every other character falls back to the interface font.
namespace FigurineFont {

/// @p base with Pragma Figurine placed before its families.
QFont apply(const QFont &base);

} // namespace FigurineFont
