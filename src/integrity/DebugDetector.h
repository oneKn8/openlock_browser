// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QString>

namespace openlock {

class DebugDetector {
public:
    DebugDetector();
    ~DebugDetector();

    bool isBeingDebugged() const;
    QString debuggerName() const;

private:
    bool checkTracerPid() const;
    bool checkPtraceSelf() const;
    bool checkDebuggerProcesses() const;

    mutable QString m_detectedDebugger;
};

} // namespace openlock
