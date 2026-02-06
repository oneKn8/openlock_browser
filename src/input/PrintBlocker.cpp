// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "input/PrintBlocker.h"

#include <QProcess>
#include <QDebug>

namespace openlock {

PrintBlocker::PrintBlocker(QObject* parent)
    : QObject(parent)
{
}

bool PrintBlocker::engage()
{
    QProcess proc;
    proc.start("systemctl", {"stop", "cups.service"});
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        m_cupsWasStopped = true;
        qInfo() << "CUPS service stopped";
    } else {
        qWarning() << "Could not stop CUPS (may need root):"
                   << proc.readAllStandardError().trimmed();
    }

    m_active = true;
    return true;
}

bool PrintBlocker::release()
{
    if (m_cupsWasStopped) {
        QProcess proc;
        proc.start("systemctl", {"start", "cups.service"});
        proc.waitForFinished(5000);
        m_cupsWasStopped = false;
        qInfo() << "CUPS service restarted";
    }

    m_active = false;
    return true;
}

} // namespace openlock
