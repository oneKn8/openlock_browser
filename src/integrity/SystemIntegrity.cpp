// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "integrity/SystemIntegrity.h"
#include "integrity/VMDetector.h"
#include "integrity/DebugDetector.h"
#include "integrity/SelfVerifier.h"

#include <QDebug>

namespace openlock {

SystemIntegrity::SystemIntegrity(QObject* parent)
    : QObject(parent)
    , m_vmDetector(std::make_unique<VMDetector>())
    , m_debugDetector(std::make_unique<DebugDetector>())
    , m_selfVerifier(std::make_unique<SelfVerifier>())
{
}

SystemIntegrity::~SystemIntegrity() = default;

IntegrityReport SystemIntegrity::performFullCheck() const
{
    IntegrityReport report;
    report.passed = true;

#ifdef OPENLOCK_VM_DETECTION
    if (m_vmDetectionEnabled) {
        auto vmResult = m_vmDetector->detect();
        if (vmResult.detected) {
            report.vmDetected = true;
            report.vmType = vmResult.hypervisorName;
            report.passed = false;
        }
    }
#endif

    if (m_debugDetectionEnabled) {
        if (m_debugDetector->isBeingDebugged()) {
            report.debuggerDetected = true;
            report.debuggerType = m_debugDetector->debuggerName();
            report.passed = false;
        }
    }

    if (!m_selfVerifier->verifyIntegrity()) {
        report.binaryTampered = true;
        report.passed = false;
    }

    if (checkLdPreload()) {
        report.ldPreloadDetected = true;
        report.passed = false;
    }

    report.injectedLibraries = m_selfVerifier->detectInjectedLibraries();
    if (!report.injectedLibraries.isEmpty()) {
        report.warnings.append("Suspicious shared libraries detected");
    }

    return report;
}

bool SystemIntegrity::checkVM() const
{
#ifdef OPENLOCK_VM_DETECTION
    return m_vmDetector->detect().detected;
#else
    return false;
#endif
}

bool SystemIntegrity::checkDebugger() const
{
    return m_debugDetector->isBeingDebugged();
}

bool SystemIntegrity::checkBinaryIntegrity() const
{
    return m_selfVerifier->verifyIntegrity();
}

bool SystemIntegrity::checkLdPreload() const
{
    const char* preload = std::getenv("LD_PRELOAD");
    return (preload != nullptr && preload[0] != '\0');
}

bool SystemIntegrity::checkProcMaps() const
{
    return !m_selfVerifier->detectInjectedLibraries().isEmpty();
}

QByteArray SystemIntegrity::binaryHash() const
{
    return m_selfVerifier->computeBinaryHash();
}

void SystemIntegrity::setVMDetectionEnabled(bool enabled) { m_vmDetectionEnabled = enabled; }
void SystemIntegrity::setDebugDetectionEnabled(bool enabled) { m_debugDetectionEnabled = enabled; }

} // namespace openlock
