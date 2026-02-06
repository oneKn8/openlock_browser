// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "input/ShortcutBlocker.h"

#include <QKeyEvent>
#include <QCoreApplication>
#include <QDebug>

namespace openlock {

ShortcutBlocker::ShortcutBlocker(QObject* parent)
    : QObject(parent)
{
}

bool ShortcutBlocker::engage()
{
    QCoreApplication::instance()->installEventFilter(this);
    m_active = true;
    qInfo() << "Shortcut blocker active";
    return true;
}

bool ShortcutBlocker::release()
{
    QCoreApplication::instance()->removeEventFilter(this);
    m_active = false;
    qInfo() << "Shortcut blocker released";
    return true;
}

bool ShortcutBlocker::eventFilter(QObject* obj, QEvent* event)
{
    if (!m_active) return false;

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers mods = keyEvent->modifiers();
        int key = keyEvent->key();

        if ((mods & Qt::AltModifier) && key == Qt::Key_Tab) {
            emit blocked("Alt+Tab");
            return true;
        }

        if ((mods & Qt::AltModifier) && key == Qt::Key_F4) {
            emit blocked("Alt+F4");
            return true;
        }

        if (key == Qt::Key_Super_L || key == Qt::Key_Super_R || key == Qt::Key_Meta) {
            emit blocked("Super");
            return true;
        }

        if (key == Qt::Key_Print || key == Qt::Key_SysReq) {
            emit blocked("PrintScreen");
            return true;
        }

        if ((mods & Qt::ControlModifier) && (mods & Qt::AltModifier) && key == Qt::Key_Delete) {
            emit blocked("Ctrl+Alt+Delete");
            return true;
        }

        if ((mods & Qt::ControlModifier) && (mods & Qt::AltModifier) &&
            key >= Qt::Key_F1 && key <= Qt::Key_F12) {
            emit blocked("Ctrl+Alt+F" + QString::number(key - Qt::Key_F1 + 1));
            return true;
        }

        if ((mods & Qt::ControlModifier) && (mods & Qt::AltModifier) && key == Qt::Key_Backspace) {
            emit blocked("Ctrl+Alt+Backspace");
            return true;
        }

        if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier) && key == Qt::Key_I) {
            emit blocked("Ctrl+Shift+I");
            return true;
        }

        if (key == Qt::Key_F12) {
            emit blocked("F12");
            return true;
        }

        if ((mods & Qt::ControlModifier) && key == Qt::Key_U) {
            emit blocked("Ctrl+U");
            return true;
        }

        if ((mods & Qt::ControlModifier) && key == Qt::Key_S) {
            emit blocked("Ctrl+S");
            return true;
        }

        if ((mods & Qt::ControlModifier) && key == Qt::Key_P) {
            emit blocked("Ctrl+P");
            return true;
        }

        if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier) && key == Qt::Key_J) {
            emit blocked("Ctrl+Shift+J");
            return true;
        }

        if ((mods & Qt::ControlModifier) && key == Qt::Key_W) {
            emit blocked("Ctrl+W");
            return true;
        }

        if ((mods & Qt::ControlModifier) && key == Qt::Key_N) {
            emit blocked("Ctrl+N");
            return true;
        }

        if ((mods & Qt::ControlModifier) && key == Qt::Key_T) {
            emit blocked("Ctrl+T");
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

} // namespace openlock
