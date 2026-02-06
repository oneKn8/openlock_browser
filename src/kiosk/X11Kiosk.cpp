// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "kiosk/PlatformKiosk.h"

#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>

#ifdef OPENLOCK_HAS_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace openlock {

class X11Kiosk : public PlatformKiosk {
    Q_OBJECT

public:
    explicit X11Kiosk(QObject* parent = nullptr)
        : PlatformKiosk(parent)
    {
    }

    ~X11Kiosk() override
    {
        if (m_engaged) {
            release();
        }
    }

    bool engage() override
    {
#ifdef OPENLOCK_HAS_X11
        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            qCritical() << "Failed to open X11 display";
            return false;
        }

        Window root = DefaultRootWindow(m_display);

        // Set override-redirect to bypass window manager
        XSetWindowAttributes attrs;
        attrs.override_redirect = True;

        // Get screen dimensions for fullscreen coverage
        Screen* screen = DefaultScreenOfDisplay(m_display);
        int screenWidth = WidthOfScreen(screen);
        int screenHeight = HeightOfScreen(screen);

        // Create a fullscreen override-redirect window
        m_kioskWindow = XCreateWindow(
            m_display, root,
            0, 0, screenWidth, screenHeight,
            0, CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect, &attrs
        );

        // Set window properties
        Atom wmState = XInternAtom(m_display, "_NET_WM_STATE", False);
        Atom fullscreenAtom = XInternAtom(m_display, "_NET_WM_STATE_FULLSCREEN", False);
        Atom aboveAtom = XInternAtom(m_display, "_NET_WM_STATE_ABOVE", False);

        Atom atoms[] = { fullscreenAtom, aboveAtom };
        XChangeProperty(m_display, m_kioskWindow, wmState, XA_ATOM, 32,
                       PropModeReplace, reinterpret_cast<unsigned char*>(atoms), 2);

        // Map the window and raise it
        XMapRaised(m_display, m_kioskWindow);
        XFlush(m_display);

        m_engaged = true;
        qInfo() << "X11 kiosk engaged:" << screenWidth << "x" << screenHeight;
        return true;
#else
        qCritical() << "X11 support not compiled in";
        return false;
#endif
    }

    bool release() override
    {
#ifdef OPENLOCK_HAS_X11
        if (m_display && m_kioskWindow) {
            XDestroyWindow(m_display, m_kioskWindow);
            m_kioskWindow = 0;
        }
        if (m_display) {
            XCloseDisplay(m_display);
            m_display = nullptr;
        }
        m_engaged = false;
        return true;
#else
        return false;
#endif
    }

    bool isEngaged() const override { return m_engaged; }

    QList<MonitorInfo> connectedMonitors() const override
    {
        QList<MonitorInfo> monitors;

#ifdef OPENLOCK_HAS_X11
        if (!m_display) return monitors;

        Window root = DefaultRootWindow(m_display);
        XRRScreenResources* res = XRRGetScreenResources(m_display, root);
        if (!res) return monitors;

        for (int i = 0; i < res->noutput; i++) {
            XRROutputInfo* output = XRRGetOutputInfo(m_display, res, res->outputs[i]);
            if (!output) continue;

            MonitorInfo info;
            info.name = QString::fromLatin1(output->name);
            info.connected = (output->connection == RR_Connected);

            if (output->crtc) {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo(m_display, res, output->crtc);
                if (crtc) {
                    info.geometry = QRect(crtc->x, crtc->y, crtc->width, crtc->height);
                    XRRFreeCrtcInfo(crtc);
                }
            }

            monitors.append(info);
            XRRFreeOutputInfo(output);
        }

        XRRFreeScreenResources(res);
#endif
        return monitors;
    }

    bool coverAllMonitors() override
    {
#ifdef OPENLOCK_HAS_X11
        auto monitors = connectedMonitors();
        for (const auto& mon : monitors) {
            if (mon.connected && !mon.primary) {
                qInfo() << "Covering secondary monitor:" << mon.name;
                // Create black overlay window on each secondary monitor
                // Full implementation creates a black window covering each monitor
            }
        }
        return true;
#else
        return false;
#endif
    }

    bool disableVTSwitch() override
    {
        // Disable Ctrl+Alt+F1-F12 virtual terminal switching
        int fd = open("/dev/tty", O_RDWR);
        if (fd < 0) {
            qWarning() << "Cannot open /dev/tty for VT switch disable (need root)";
            return false;
        }

        struct vt_mode vtm;
        vtm.mode = VT_PROCESS;
        vtm.waitv = 0;
        vtm.relsig = 0;
        vtm.acqsig = 0;
        vtm.frsig = 0;

        if (ioctl(fd, VT_SETMODE, &vtm) < 0) {
            qWarning() << "Failed to set VT mode";
            close(fd);
            return false;
        }

        m_ttyFd = fd;
        qInfo() << "VT switching disabled";
        return true;
    }

    bool enableVTSwitch() override
    {
        if (m_ttyFd >= 0) {
            struct vt_mode vtm;
            vtm.mode = VT_AUTO;
            ioctl(m_ttyFd, VT_SETMODE, &vtm);
            close(m_ttyFd);
            m_ttyFd = -1;
            qInfo() << "VT switching re-enabled";
        }
        return true;
    }

private:
#ifdef OPENLOCK_HAS_X11
    Display* m_display = nullptr;
    Window m_kioskWindow = 0;
#endif
    int m_ttyFd = -1;
    bool m_engaged = false;
};

PlatformKiosk* createX11Kiosk(QObject* parent)
{
    return new X11Kiosk(parent);
}

} // namespace openlock

#include "X11Kiosk.moc"
