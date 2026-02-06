// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "browser/DownloadBlocker.h"

// Download blocking is handled in SecureBrowser::setupProfile()
// via the QWebEngineProfile::downloadRequested signal.
// This file exists for the build system.

namespace openlock {
// Intentionally empty — functionality is in SecureBrowser.cpp
} // namespace openlock
