#pragma once

#include <QIcon>
#include <QString>

/// Flat, single-color icons drawn in code.
///
/// They are painted with the palette's text color at the moment they are
/// shown: white-ish on dark themes, dark on light themes, dimmed when
/// disabled, and they follow theme changes without reloading anything.
namespace SymbolicIcons {

/// Icon for a freedesktop icon name (e.g. "go-next", "document-open").
/// Returns a null icon for names without a symbolic drawing.
QIcon icon(const QString &name);

} // namespace SymbolicIcons
