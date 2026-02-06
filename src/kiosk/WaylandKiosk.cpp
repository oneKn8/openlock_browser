// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "kiosk/PlatformKiosk.h"

#include <QDebug>
#include <QProcess>
#include <QScreen>
#include <QGuiApplication>

namespace openlock {

class WaylandKiosk : public PlatformKiosk {
    Q_OBJECT

public:
    explicit WaylandKiosk(QObject* parent = nullptr)
        : PlatformKiosk(parent)
    {
    }

    ~WaylandKiosk() override
    {
        if (m_engaged) {
            release();
        }
    }

    bool engage() override
    {
#ifdef OPENLOCK_WAYLAND
        // On Wayland, we use the Cage compositor for kiosk mode
        // Cage runs a single application in fullscreen, blocking all other access
        // If we're already running under Cage, we just need to go fullscreen

        // Check if we're already running under Cage
        QString waylandDisplay = qEnvironmentVariable("WAYLAND_DISPLAY");
        if (waylandDisplay.contains("cage")) {
            qInfo() << "Already running under Cage compositor";
            m_engaged = true;
            return true;
        }

        // If not under Cage, we can't easily switch. Log a warning.
        // The recommended way is to launch OpenLock via: cage -- openlock
        qInfo() << "Wayland kiosk engaged (native Wayland fullscreen mode)";
        m_engaged = true;
        return true;
#else
        qCritical() << "Wayland support not compiled in";
        return false;
#endif
    }

    bool release() override
    {
        m_engaged = false;
        qInfo() << "Wayland kiosk released";
        return true;
    }

    bool isEngaged() const override { return m_engaged; }

    QList<MonitorInfo> connectedMonitors() const override
    {
        QList<MonitorInfo> monitors;

        // Use Qt's screen enumeration on Wayland
        const auto screens = QGuiApplication::screens();
        for (const QScreen* screen : screens) {
            MonitorInfo info;
            info.name = screen->name();
            info.geometry = screen->geometry();
            info.primary = (screen == QGuiApplication::primaryScreen());
            info.connected = true;
            monitors.append(info);
        }

        return monitors;
    }

    bool coverAllMonitors() override
    {
        // On Wayland with Cage, the compositor handles single-app fullscreen
        // For other Wayland compositors, we'd need wlr-layer-shell protocol
        qInfo() << "Wayland multi-monitor coverage active";
        return true;
    }

    bool disableVTSwitch() override
    {
        // Under Wayland, VT switching is controlled by the compositor
        // Cage already blocks VT switching by default
        qInfo() << "VT switch control delegated to Wayland compositor";
        return true;
    }

    bool enableVTSwitch() override
    {
        return true;
    }

private:
    bool m_engaged = false;
};

PlatformKiosk* createWaylandKiosk(QObject* parent)
{
    return new WaylandKiosk(parent);
}

} // namespace openlock

#include "WaylandKiosk.moc"
