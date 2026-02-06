// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QSet>
#include <vector>

namespace openlock {

struct ProcessInfo {
    int pid = 0;
    QString name;
    QString cmdline;
    QString exe;
    int uid = -1;
};

class ProcessBlocklist;

class ProcessGuard : public QObject {
    Q_OBJECT

public:
    explicit ProcessGuard(QObject* parent = nullptr);
    ~ProcessGuard() override;

    bool initialize(const QString& blocklistPath);
    void addToBlocklist(const QString& processName);
    void addToAllowlist(const QString& processName);

    std::vector<ProcessInfo> scanForBlockedProcesses() const;
    bool startMonitoring(int intervalMs = 1000);
    void stopMonitoring();
    bool isMonitoring() const;

    bool killProcess(int pid);

signals:
    void blockedProcessFound(const ProcessInfo& proc);
    void blockedProcessKilled(const ProcessInfo& proc);
    void monitoringStarted();
    void monitoringStopped();

private slots:
    void performScan();

private:
    std::vector<ProcessInfo> enumerateProcesses() const;
    bool isBlocked(const ProcessInfo& proc) const;

    std::unique_ptr<ProcessBlocklist> m_blocklist;
    QSet<QString> m_allowlist;
    QTimer* m_timer = nullptr;
    bool m_monitoring = false;
};

} // namespace openlock
