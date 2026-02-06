// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "browser/DevToolsBlocker.h"

// DevTools blocking is handled by:
// - ShortcutBlocker: intercepts F12, Ctrl+Shift+I, Ctrl+Shift+J
// - SecurePage::javaScriptConsoleMessage: suppresses console output
// - Environment: QTWEBENGINE_REMOTE_DEBUGGING is never set
// This file exists for the build system.

namespace openlock {
// Intentionally empty
} // namespace openlock
