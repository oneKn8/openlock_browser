// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

namespace openlock {

class VMDetector;
class DebugDetector;
class SelfVerifier;

struct IntegrityReport {
    bool passed = false;
    bool vmDetected = false;
    QString vmType;
    bool debuggerDetected = false;
    QString debuggerType;
    bool binaryTampered = false;
    bool ldPreloadDetected = false;
    QStringList injectedLibraries;
    QStringList warnings;
};

class SystemIntegrity : public QObject {
    Q_OBJECT

public:
    explicit SystemIntegrity(QObject* parent = nullptr);
    ~SystemIntegrity() override;

    IntegrityReport performFullCheck() const;

    bool checkVM() const;
    bool checkDebugger() const;
    bool checkBinaryIntegrity() const;
    bool checkLdPreload() const;
    bool checkProcMaps() const;

    QByteArray binaryHash() const;

    void setVMDetectionEnabled(bool enabled);
    void setDebugDetectionEnabled(bool enabled);

signals:
    void integrityViolation(const QString& description);

private:
    std::unique_ptr<VMDetector> m_vmDetector;
    std::unique_ptr<DebugDetector> m_debugDetector;
    std::unique_ptr<SelfVerifier> m_selfVerifier;
    bool m_vmDetectionEnabled = true;
    bool m_debugDetectionEnabled = true;
};

} // namespace openlock
