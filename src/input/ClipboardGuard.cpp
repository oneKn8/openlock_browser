// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "input/ClipboardGuard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>
#include <QDebug>

namespace openlock {

ClipboardGuard::ClipboardGuard(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ClipboardGuard::clearClipboard);
}

bool ClipboardGuard::engage()
{
    clearClipboard();

    m_timer->start(500);

    QClipboard* clipboard = QGuiApplication::clipboard();
    connect(clipboard, &QClipboard::changed, this, [this]() {
        if (m_active) {
            clearClipboard();
            emit violation();
        }
    });

    m_active = true;
    qInfo() << "Clipboard guard active";
    return true;
}

bool ClipboardGuard::release()
{
    m_timer->stop();
    m_active = false;
    qInfo() << "Clipboard guard released";
    return true;
}

void ClipboardGuard::clearClipboard()
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->clear(QClipboard::Clipboard);
        clipboard->clear(QClipboard::Selection);
        clipboard->clear(QClipboard::FindBuffer);
    }
}

} // namespace openlock
