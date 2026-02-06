// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <memory>

namespace openlock {

class Config;
class KioskShell;
class ProcessGuard;
class InputLockdown;
class SystemIntegrity;
class SecureBrowser;
class SEBProtocol;

enum class LockdownState {
    Idle,
    Initializing,
    PreCheck,       // Checking system integrity, blocking processes
    Locked,         // Full lockdown active
    ExamActive,     // Exam in progress
    ShuttingDown,   // Releasing lockdown
    Error
};

class LockdownEngine : public QObject {
    Q_OBJECT

public:
    explicit LockdownEngine(QObject* parent = nullptr);
    ~LockdownEngine() override;

    bool initialize(const QString& configPath);
    bool engageLockdown();
    bool releaseLockdown(const QString& exitPassword = {});
    LockdownState state() const;

    Config* config() const;
    SecureBrowser* browser() const;

signals:
    void stateChanged(LockdownState newState);
    void lockdownEngaged();
    void lockdownReleased();
    void errorOccurred(const QString& message);
    void blockedProcessDetected(const QString& processName, int pid);

private:
    bool performPreChecks();
    bool startKiosk();
    bool startProcessGuard();
    bool startInputLockdown();
    bool checkSystemIntegrity();

    LockdownState m_state = LockdownState::Idle;
    std::unique_ptr<Config> m_config;
    std::unique_ptr<KioskShell> m_kiosk;
    std::unique_ptr<ProcessGuard> m_processGuard;
    std::unique_ptr<InputLockdown> m_inputLockdown;
    std::unique_ptr<SystemIntegrity> m_integrity;
    std::unique_ptr<SecureBrowser> m_browser;
    std::unique_ptr<SEBProtocol> m_sebProtocol;
};

} // namespace openlock
