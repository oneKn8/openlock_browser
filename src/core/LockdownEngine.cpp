// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "core/LockdownEngine.h"
#include "core/Config.h"
#include "kiosk/KioskShell.h"
#include "guard/ProcessGuard.h"
#include "input/InputLockdown.h"
#include "integrity/SystemIntegrity.h"
#include "browser/SecureBrowser.h"
#include "protocol/SEBProtocol.h"
#include "protocol/SEBRequestInterceptor.h"

#include <QDebug>
#include <QFile>
#include <QWebEngineProfile>
#include <QCoreApplication>

namespace openlock {

LockdownEngine::LockdownEngine(QObject* parent)
    : QObject(parent)
    , m_config(std::make_unique<Config>(this))
    , m_kiosk(std::make_unique<KioskShell>(this))
    , m_processGuard(std::make_unique<ProcessGuard>(this))
    , m_inputLockdown(std::make_unique<InputLockdown>(this))
    , m_integrity(std::make_unique<SystemIntegrity>(this))
    , m_browser(std::make_unique<SecureBrowser>())
    , m_sebProtocol(std::make_unique<SEBProtocol>(this))
{
    connect(m_processGuard.get(), &ProcessGuard::blockedProcessFound,
            this, [this](const ProcessInfo& proc) {
        emit blockedProcessDetected(proc.name, proc.pid);
    });
}

LockdownEngine::~LockdownEngine()
{
    if (m_state == LockdownState::Locked || m_state == LockdownState::ExamActive) {
        releaseLockdown();
    }
}

bool LockdownEngine::initialize(const QString& configPath)
{
    m_state = LockdownState::Initializing;
    emit stateChanged(m_state);

    // Load configuration
    if (!configPath.isEmpty()) {
        if (!m_config->loadFromFile(configPath)) {
            m_state = LockdownState::Error;
            emit errorOccurred("Failed to load config: " + configPath);
            return false;
        }
    }

    // Initialize browser with config
    if (!m_browser->initialize(m_config.get())) {
        m_state = LockdownState::Error;
        emit errorOccurred("Failed to initialize browser");
        return false;
    }

    // Initialize SEB protocol and request interceptor
    const auto& examConfig = m_config->examConfig();
    if (examConfig.sebMode) {
        if (!m_sebProtocol->initialize(m_config.get())) {
            m_state = LockdownState::Error;
            emit errorOccurred("Failed to initialize SEB protocol");
            return false;
        }
    }

    // Install URL request interceptor for SEB headers and URL filtering
    auto* interceptor = new SEBRequestInterceptor(this);
    if (examConfig.sebMode) {
        interceptor->setSEBProtocol(m_sebProtocol.get());
    }
    if (m_browser->navigationFilter()) {
        interceptor->setNavigationFilter(m_browser->navigationFilter());
    }
    // Get the profile from the browser's web view and install interceptor
    if (m_browser->webView() && m_browser->webView()->page()) {
        m_browser->webView()->page()->profile()->setUrlRequestInterceptor(interceptor);
    }

    // Initialize kiosk shell
    if (!m_kiosk->initialize()) {
        qWarning() << "Kiosk shell initialization failed, continuing without kiosk";
    }

    // Initialize process guard with blocklist
    // Try build-local path first, then installed path
    QString blocklistPath = QCoreApplication::applicationDirPath() + "/share/openlock/blocklist.json";
    if (!QFile::exists(blocklistPath)) {
        blocklistPath = QCoreApplication::applicationDirPath() + "/../share/openlock/blocklist.json";
    }
    if (!m_processGuard->initialize(blocklistPath)) {
        qWarning() << "Process guard initialization failed";
    }

    m_state = LockdownState::Idle;
    emit stateChanged(m_state);
    return true;
}

bool LockdownEngine::engageLockdown()
{
    m_state = LockdownState::PreCheck;
    emit stateChanged(m_state);

    if (!performPreChecks()) {
        m_state = LockdownState::Error;
        emit errorOccurred("Pre-checks failed");
        return false;
    }

    if (!startKiosk()) {
        qWarning() << "Kiosk engagement failed, continuing";
    }

    if (!startProcessGuard()) {
        qWarning() << "Process guard start failed, continuing";
    }

    if (!startInputLockdown()) {
        qWarning() << "Input lockdown failed, continuing";
    }

    m_state = LockdownState::Locked;
    emit stateChanged(m_state);
    emit lockdownEngaged();

    qInfo() << "Lockdown engaged successfully";
    return true;
}

bool LockdownEngine::releaseLockdown(const QString& exitPassword)
{
    const auto& examConfig = m_config->examConfig();

    // Check exit password if configured
    if (!examConfig.exitPassword.isEmpty() && exitPassword != examConfig.exitPassword) {
        emit errorOccurred("Incorrect exit password");
        return false;
    }

    m_state = LockdownState::ShuttingDown;
    emit stateChanged(m_state);

    m_processGuard->stopMonitoring();
    m_inputLockdown->release();
    m_kiosk->release();

    m_state = LockdownState::Idle;
    emit stateChanged(m_state);
    emit lockdownReleased();

    qInfo() << "Lockdown released";
    return true;
}

LockdownState LockdownEngine::state() const { return m_state; }
Config* LockdownEngine::config() const { return m_config.get(); }
SecureBrowser* LockdownEngine::browser() const { return m_browser.get(); }

bool LockdownEngine::performPreChecks()
{
    if (!checkSystemIntegrity()) {
        return false;
    }

    // Scan for blocked processes
    auto blocked = m_processGuard->scanForBlockedProcesses();
    if (!blocked.empty()) {
        for (const auto& proc : blocked) {
            emit blockedProcessDetected(proc.name, proc.pid);
        }
        emit errorOccurred(
            QString("Found %1 blocked process(es). Please close them before starting the exam.")
                .arg(blocked.size()));
        return false;
    }

    return true;
}

bool LockdownEngine::startKiosk()
{
    return m_kiosk->engage();
}

bool LockdownEngine::startProcessGuard()
{
    return m_processGuard->startMonitoring();
}

bool LockdownEngine::startInputLockdown()
{
    return m_inputLockdown->engage();
}

bool LockdownEngine::checkSystemIntegrity()
{
    auto report = m_integrity->performFullCheck();

    if (!report.passed) {
        if (report.vmDetected) {
            emit errorOccurred("Virtual machine detected: " + report.vmType +
                             ". Exams cannot be taken in a virtual machine.");
        }
        if (report.debuggerDetected) {
            emit errorOccurred("Debugger detected: " + report.debuggerType +
                             ". Please detach all debuggers.");
        }
        if (report.binaryTampered) {
            emit errorOccurred("Binary integrity check failed. The application may have been modified.");
        }
        if (report.ldPreloadDetected) {
            emit errorOccurred("LD_PRELOAD detected. Library injection is not allowed.");
        }
        return false;
    }

    return true;
}

} // namespace openlock
