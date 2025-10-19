#pragma once

#define FLUX_VERSION_MAJOR 0
#define FLUX_VERSION_MINOR 1
#define FLUX_VERSION_PATCH 0

// Core
#include "flux/core/browser.h"

// UI
#include "flux/ui/icon_provider.h"
#include "flux/ui/renderer.h"
#include "flux/ui/theme.h"

// Future additions:
// #include "flux/core/file_ops.h"      // File operations (copy, paste, delete)
// #include "flux/core/selection.h"     // Multi-select
// #include "flux/ui/preview.h"         // Preview pane
// #include "flux/config/config.h"      // Configuration loading
// #include "flux/config/keybindings.h" // Custom keybindings

namespace flux {

// Version info
inline const char *getVersion() { return "0.1.0"; }

inline int getVersionMajor() { return FLUX_VERSION_MAJOR; }
inline int getVersionMinor() { return FLUX_VERSION_MINOR; }
inline int getVersionPatch() { return FLUX_VERSION_PATCH; }

} // namespace flux