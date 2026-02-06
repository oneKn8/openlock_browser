// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "integrity/DebugDetector.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>

#include <sys/ptrace.h>
#include <unistd.h>

namespace openlock {

DebugDetector::DebugDetector() = default;
DebugDetector::~DebugDetector() = default;

bool DebugDetector::isBeingDebugged() const
{
    return checkTracerPid() || checkPtraceSelf() || checkDebuggerProcesses();
}

QString DebugDetector::debuggerName() const
{
    return m_detectedDebugger;
}

bool DebugDetector::checkTracerPid() const
{
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly)) return false;

    QTextStream stream(&file);
    QString line;
    while (stream.readLineInto(&line)) {
        if (line.startsWith("TracerPid:")) {
            int tracerPid = line.mid(10).trimmed().toInt();
            if (tracerPid != 0) {
                // Read the tracer's name
                QFile commFile(QString("/proc/%1/comm").arg(tracerPid));
                if (commFile.open(QIODevice::ReadOnly)) {
                    m_detectedDebugger = QTextStream(&commFile).readLine().trimmed();
                } else {
                    m_detectedDebugger = QString("PID %1").arg(tracerPid);
                }
                qWarning() << "Tracer detected:" << m_detectedDebugger << "(PID" << tracerPid << ")";
                return true;
            }
            break;
        }
    }

    return false;
}

bool DebugDetector::checkPtraceSelf() const
{
    // Try to ptrace ourselves — fails if already being traced
    long result = ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
    if (result == -1) {
        m_detectedDebugger = "ptrace attached";
        qWarning() << "PTRACE_TRACEME failed — already being traced";
        return true;
    }

    // Detach from ourselves
    ptrace(PTRACE_DETACH, 0, nullptr, nullptr);
    return false;
}

bool DebugDetector::checkDebuggerProcesses() const
{
    QStringList debuggers = {"gdb", "lldb", "strace", "ltrace", "radare2", "r2", "ida"};

    QDir procDir("/proc");
    const auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        bool ok;
        entry.toInt(&ok);
        if (!ok) continue;

        QFile commFile(QString("/proc/%1/comm").arg(entry));
        if (!commFile.open(QIODevice::ReadOnly)) continue;

        QString name = QTextStream(&commFile).readLine().trimmed().toLower();
        if (debuggers.contains(name)) {
            m_detectedDebugger = name;
            qWarning() << "Debugger process found:" << name;
            return true;
        }
    }

    return false;
}

} // namespace openlock
