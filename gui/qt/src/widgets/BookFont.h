#pragma once

#include <QFont>

/// The face of text written between the moves of a chapter, as chess books
/// set it: Crimson Pro ExtraLight (shipped, SIL Open Font License), a third
/// larger than the interface, with the figurines before it for the piece symbols.
namespace BookFont {

/// The paragraph font, from the interface font `base`.
QFont paragraph(const QFont &base);

/// Line height of a paragraph, in per cent of the font's.
inline constexpr int lineHeight = 125;
/// The indent of the first line of each paragraph, in pixels, as in books.
qreal indent(const QFont &paragraphFont);

} // namespace BookFont
