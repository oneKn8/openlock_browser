// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>

#include <unistd.h>
#include <sys/types.h>

namespace openlock {

class CGroupIsolator {
public:
    CGroupIsolator() = default;
    ~CGroupIsolator() { release(); }

    bool isolate()
    {
        // Create a cgroup v2 for OpenLock
        // This prevents new processes from being spawned outside our cgroup
        m_cgroupPath = "/sys/fs/cgroup/openlock-exam";

        QDir dir;
        if (!dir.mkpath(m_cgroupPath)) {
            qWarning() << "Failed to create cgroup (need root):" << m_cgroupPath;
            return false;
        }

        // Move our PID into the cgroup
        QFile procsFile(m_cgroupPath + "/cgroup.procs");
        if (!procsFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to write to cgroup.procs";
            return false;
        }

        QTextStream stream(&procsFile);
        stream << getpid();
        procsFile.close();

        // Set max pids to prevent fork bombs and new process creation
        // Allow enough for QtWebEngine's multi-process model
        QFile maxFile(m_cgroupPath + "/pids.max");
        if (maxFile.open(QIODevice::WriteOnly)) {
            QTextStream(&maxFile) << "50"; // QtWebEngine needs ~20-30 processes
            maxFile.close();
        }

        m_active = true;
        qInfo() << "CGroup isolation active:" << m_cgroupPath;
        return true;
    }

    bool release()
    {
        if (!m_active) return true;

        // Move processes back to root cgroup
        QFile rootProcs("/sys/fs/cgroup/cgroup.procs");
        if (rootProcs.open(QIODevice::WriteOnly)) {
            QTextStream(&rootProcs) << getpid();
            rootProcs.close();
        }

        // Remove our cgroup directory
        QDir().rmdir(m_cgroupPath);

        m_active = false;
        qInfo() << "CGroup isolation released";
        return true;
    }

    bool isActive() const { return m_active; }

private:
    QString m_cgroupPath;
    bool m_active = false;
};

} // namespace openlock
