// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QRect>
#include <QList>

namespace openlock {

struct MonitorInfo {
    QString name;
    QRect geometry;
    bool primary = false;
    bool connected = false;
};

class PlatformKiosk : public QObject {
    Q_OBJECT

public:
    explicit PlatformKiosk(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PlatformKiosk() = default;

    virtual bool engage() = 0;
    virtual bool release() = 0;
    virtual bool isEngaged() const = 0;

    virtual QList<MonitorInfo> connectedMonitors() const = 0;
    virtual bool coverAllMonitors() = 0;
    virtual bool disableVTSwitch() = 0;
    virtual bool enableVTSwitch() = 0;

signals:
    void monitorHotplug(const MonitorInfo& monitor, bool connected);
    void kioskEscapeAttempt(const QString& method);
};

} // namespace openlock
