// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "kiosk/KioskShell.h"
#include "kiosk/PlatformKiosk.h"

#include <QGuiApplication>
#include <QDebug>
#include <cstdlib>

// Forward declarations for platform implementations
namespace openlock {
PlatformKiosk* createX11Kiosk(QObject* parent);
PlatformKiosk* createWaylandKiosk(QObject* parent);
}

namespace openlock {

KioskShell::KioskShell(QObject* parent)
    : QObject(parent)
{
}

KioskShell::~KioskShell()
{
    if (m_engaged) {
        release();
    }
}

DisplayServer KioskShell::detectDisplayServer()
{
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* x11Display = std::getenv("DISPLAY");

    // Check QGuiApplication platform name
    QString platform = QGuiApplication::platformName();
    if (platform == "wayland" || platform == "wayland-egl") {
        return DisplayServer::Wayland;
    }
    if (platform == "xcb" || platform == "x11") {
        return DisplayServer::X11;
    }

    // Fallback to environment variables
    if (waylandDisplay && waylandDisplay[0] != '\0') {
        return DisplayServer::Wayland;
    }
    if (x11Display && x11Display[0] != '\0') {
        return DisplayServer::X11;
    }

    return DisplayServer::Unknown;
}

bool KioskShell::initialize()
{
    m_displayServer = detectDisplayServer();

    switch (m_displayServer) {
    case DisplayServer::X11:
        qInfo() << "Detected X11 display server";
        m_platformKiosk.reset(createX11Kiosk(this));
        break;
    case DisplayServer::Wayland:
        qInfo() << "Detected Wayland display server";
        m_platformKiosk.reset(createWaylandKiosk(this));
        break;
    default:
        qWarning() << "Unknown display server, falling back to X11";
        m_platformKiosk.reset(createX11Kiosk(this));
        break;
    }

    if (m_platformKiosk) {
        connect(m_platformKiosk.get(), &PlatformKiosk::kioskEscapeAttempt,
                this, &KioskShell::escapeAttempt);
    }

    return m_platformKiosk != nullptr;
}

bool KioskShell::engage()
{
    if (!m_platformKiosk) {
        qCritical() << "Kiosk not initialized";
        return false;
    }

    if (m_platformKiosk->engage()) {
        m_engaged = true;
        m_platformKiosk->coverAllMonitors();
        m_platformKiosk->disableVTSwitch();
        emit engaged();
        return true;
    }

    return false;
}

bool KioskShell::release()
{
    if (!m_platformKiosk || !m_engaged) {
        return true;
    }

    m_platformKiosk->enableVTSwitch();
    bool result = m_platformKiosk->release();
    m_engaged = false;
    emit released();
    return result;
}

bool KioskShell::isEngaged() const { return m_engaged; }
PlatformKiosk* KioskShell::platformKiosk() const { return m_platformKiosk.get(); }

} // namespace openlock
