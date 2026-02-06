// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "input/InputLockdown.h"
#include "input/ClipboardGuard.h"
#include "input/ShortcutBlocker.h"
#include "input/PrintBlocker.h"

#include <QDebug>

#ifdef OPENLOCK_HAS_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

namespace openlock {

InputLockdown::InputLockdown(QObject* parent)
    : QObject(parent)
    , m_clipboardGuard(std::make_unique<ClipboardGuard>(this))
    , m_shortcutBlocker(std::make_unique<ShortcutBlocker>(this))
    , m_printBlocker(std::make_unique<PrintBlocker>(this))
{
    connect(m_clipboardGuard.get(), &ClipboardGuard::violation,
            this, &InputLockdown::clipboardViolation);
    connect(m_shortcutBlocker.get(), &ShortcutBlocker::blocked,
            this, &InputLockdown::shortcutBlocked);
}

InputLockdown::~InputLockdown()
{
    if (m_engaged) {
        release();
    }
}

bool InputLockdown::engage()
{
    bool success = true;

    if (!grabKeyboard()) {
        qWarning() << "Keyboard grab failed";
        success = false;
    }

    if (!m_clipboardGuard->engage()) {
        qWarning() << "Clipboard guard failed";
    }

    if (!m_shortcutBlocker->engage()) {
        qWarning() << "Shortcut blocker failed";
    }

    if (!m_printBlocker->engage()) {
        qWarning() << "Print blocker failed";
    }

    m_engaged = true;
    emit engaged();
    qInfo() << "Input lockdown engaged";
    return success;
}

bool InputLockdown::release()
{
    ungrabKeyboard();
    m_clipboardGuard->release();
    m_shortcutBlocker->release();
    m_printBlocker->release();

    m_engaged = false;
    emit released();
    qInfo() << "Input lockdown released";
    return true;
}

bool InputLockdown::isEngaged() const { return m_engaged; }

void InputLockdown::setClipboardAllowed(bool allowed)
{
    if (allowed) {
        m_clipboardGuard->release();
    } else {
        m_clipboardGuard->engage();
    }
}

void InputLockdown::setPrintAllowed(bool allowed)
{
    if (allowed) {
        m_printBlocker->release();
    } else {
        m_printBlocker->engage();
    }
}

ClipboardGuard* InputLockdown::clipboardGuard() const { return m_clipboardGuard.get(); }
ShortcutBlocker* InputLockdown::shortcutBlocker() const { return m_shortcutBlocker.get(); }

bool InputLockdown::grabKeyboard()
{
#ifdef OPENLOCK_HAS_X11
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    Window root = DefaultRootWindow(display);

    int result = XGrabKeyboard(display, root, True,
                                GrabModeAsync, GrabModeAsync, CurrentTime);

    if (result != GrabSuccess) {
        qWarning() << "XGrabKeyboard failed with code:" << result;
        XCloseDisplay(display);
        return false;
    }

    qInfo() << "X11 keyboard grabbed";
    XCloseDisplay(display);
    return true;
#else
    qInfo() << "Keyboard grab: Wayland compositor handles this";
    return true;
#endif
}

bool InputLockdown::ungrabKeyboard()
{
#ifdef OPENLOCK_HAS_X11
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    XUngrabKeyboard(display, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);

    qInfo() << "X11 keyboard ungrabbed";
    return true;
#else
    return true;
#endif
}

} // namespace openlock
