// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

namespace openlock {

class DevToolsBlocker {
public:
    DevToolsBlocker() = default;
    ~DevToolsBlocker() = default;
    // DevTools are blocked by:
    // 1. ShortcutBlocker intercepting F12, Ctrl+Shift+I, Ctrl+Shift+J
    // 2. SecurePage suppressing console messages
    // 3. Not setting QTWEBENGINE_REMOTE_DEBUGGING env var
};

} // namespace openlock
