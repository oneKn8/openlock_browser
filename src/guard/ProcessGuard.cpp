// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "guard/ProcessGuard.h"
#include "guard/ProcessBlocklist.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <csignal>
#include <unistd.h>

namespace openlock {

ProcessGuard::ProcessGuard(QObject* parent)
    : QObject(parent)
    , m_blocklist(std::make_unique<ProcessBlocklist>())
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ProcessGuard::performScan);
}

ProcessGuard::~ProcessGuard()
{
    stopMonitoring();
}

bool ProcessGuard::initialize(const QString& blocklistPath)
{
    return m_blocklist->loadFromFile(blocklistPath);
}

void ProcessGuard::addToBlocklist(const QString& processName)
{
    m_blocklist->add(processName);
}

void ProcessGuard::addToAllowlist(const QString& processName)
{
    m_allowlist.insert(processName.toLower());
}

std::vector<ProcessInfo> ProcessGuard::scanForBlockedProcesses() const
{
    std::vector<ProcessInfo> blocked;
    auto allProcs = enumerateProcesses();

    for (const auto& proc : allProcs) {
        if (isBlocked(proc)) {
            blocked.push_back(proc);
        }
    }

    return blocked;
}

bool ProcessGuard::startMonitoring(int intervalMs)
{
    if (m_monitoring) return true;

    m_timer->start(intervalMs);
    m_monitoring = true;
    emit monitoringStarted();
    qInfo() << "Process monitoring started (interval:" << intervalMs << "ms)";
    return true;
}

void ProcessGuard::stopMonitoring()
{
    if (!m_monitoring) return;

    m_timer->stop();
    m_monitoring = false;
    emit monitoringStopped();
    qInfo() << "Process monitoring stopped";
}

bool ProcessGuard::isMonitoring() const { return m_monitoring; }

bool ProcessGuard::killProcess(int pid)
{
    if (pid <= 0) return false;

    // First try SIGTERM for graceful shutdown
    if (::kill(pid, SIGTERM) == 0) {
        qInfo() << "Sent SIGTERM to PID" << pid;
        // Give it a moment, then force kill if needed
        usleep(500000); // 500ms
        if (::kill(pid, 0) == 0) {
            // Still alive, force kill
            ::kill(pid, SIGKILL);
            qInfo() << "Sent SIGKILL to PID" << pid;
        }
        return true;
    }

    qWarning() << "Failed to kill PID" << pid;
    return false;
}

void ProcessGuard::performScan()
{
    auto allProcs = enumerateProcesses();

    for (const auto& proc : allProcs) {
        if (isBlocked(proc)) {
            emit blockedProcessFound(proc);
            qWarning() << "Blocked process detected:" << proc.name << "(PID:" << proc.pid << ")";

            // Kill it
            if (killProcess(proc.pid)) {
                emit blockedProcessKilled(proc);
            }
        }
    }
}

std::vector<ProcessInfo> ProcessGuard::enumerateProcesses() const
{
    std::vector<ProcessInfo> processes;
    QDir procDir("/proc");

    const auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        bool ok;
        int pid = entry.toInt(&ok);
        if (!ok) continue;

        ProcessInfo info;
        info.pid = pid;

        // Read process name from /proc/[pid]/comm
        QFile commFile(QString("/proc/%1/comm").arg(pid));
        if (commFile.open(QIODevice::ReadOnly)) {
            info.name = QTextStream(&commFile).readLine().trimmed();
        }

        // Read full command line from /proc/[pid]/cmdline
        QFile cmdFile(QString("/proc/%1/cmdline").arg(pid));
        if (cmdFile.open(QIODevice::ReadOnly)) {
            QByteArray cmdline = cmdFile.readAll();
            cmdline.replace('\0', ' ');
            info.cmdline = QString::fromLocal8Bit(cmdline).trimmed();
        }

        // Read exe symlink
        QString exeLink = QFile::symLinkTarget(QString("/proc/%1/exe").arg(pid));
        info.exe = exeLink;

        // Read UID from /proc/[pid]/status
        QFile statusFile(QString("/proc/%1/status").arg(pid));
        if (statusFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&statusFile);
            QString line;
            while (stream.readLineInto(&line)) {
                if (line.startsWith("Uid:")) {
                    QStringList parts = line.split('\t', Qt::SkipEmptyParts);
                    if (parts.size() >= 2) {
                        info.uid = parts[1].toInt();
                    }
                    break;
                }
            }
        }

        if (!info.name.isEmpty()) {
            processes.push_back(info);
        }
    }

    return processes;
}

bool ProcessGuard::isBlocked(const ProcessInfo& proc) const
{
    // Skip our own process
    if (proc.pid == getpid()) return false;

    // Check allowlist first
    if (m_allowlist.contains(proc.name.toLower())) return false;

    // Check blocklist
    return m_blocklist->isBlocked(proc.name, proc.cmdline, proc.exe);
}

} // namespace openlock
