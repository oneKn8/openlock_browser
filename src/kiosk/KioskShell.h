// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "kiosk/PlatformKiosk.h"
#include <memory>

namespace openlock {

enum class DisplayServer {
    X11,
    Wayland,
    Unknown
};

class KioskShell : public QObject {
    Q_OBJECT

public:
    explicit KioskShell(QObject* parent = nullptr);
    ~KioskShell() override;

    static DisplayServer detectDisplayServer();

    bool initialize();
    bool engage();
    bool release();
    bool isEngaged() const;

    PlatformKiosk* platformKiosk() const;

signals:
    void engaged();
    void released();
    void escapeAttempt(const QString& method);

private:
    std::unique_ptr<PlatformKiosk> m_platformKiosk;
    DisplayServer m_displayServer = DisplayServer::Unknown;
    bool m_engaged = false;
};

} // namespace openlock
