#pragma once

#include "app/GraphicsSettings.h"

/// Light or dark for the whole application: as the system says, or forced
/// either way (Options ▸ Graphics Settings…).
namespace Appearance {

/// Gives the whole application the mode's colours at once. With Qt 6.8 and
/// later the platform is asked for its own light or dark look; a platform
/// that does not do it, and older Qt, get a palette in GNOME's colours.
/// Nothing happens when the mode is the one already in use.
void apply(AppearanceMode mode);

} // namespace Appearance
