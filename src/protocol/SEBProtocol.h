// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QUrl>
#include <QByteArray>
#include <memory>

namespace openlock {

class BrowserExamKey;
class ConfigKeyGenerator;
class Config;

class SEBProtocol : public QObject {
    Q_OBJECT

public:
    explicit SEBProtocol(QObject* parent = nullptr);
    ~SEBProtocol() override;

    bool initialize(const Config* config);

    // Compute per-request header values (already hex-encoded)
    QByteArray computeRequestHash(const QUrl& requestUrl) const;
    QByteArray computeConfigKeyHash(const QUrl& requestUrl) const;

    static QString requestHashHeaderName();   // X-SafeExamBrowser-RequestHash
    static QString configKeyHeaderName();     // X-SafeExamBrowser-ConfigKeyHash
    static QString sebUserAgent();

signals:
    void protocolError(const QString& message);

private:
    std::unique_ptr<BrowserExamKey> m_examKey;
    std::unique_ptr<ConfigKeyGenerator> m_configKey;
};

} // namespace openlock
