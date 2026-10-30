// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "protocol/SEBProtocol.h"
#include "protocol/BrowserExamKey.h"
#include "protocol/ConfigKeyGenerator.h"
#include "core/Config.h"

#include <QCoreApplication>
#include <QDebug>

namespace openlock {

SEBProtocol::SEBProtocol(QObject* parent)
    : QObject(parent)
    , m_examKey(std::make_unique<BrowserExamKey>())
    , m_configKey(std::make_unique<ConfigKeyGenerator>())
{
}

SEBProtocol::~SEBProtocol() = default;

bool SEBProtocol::initialize(const Config* config)
{
    if (!config) {
        emit protocolError("No configuration provided");
        return false;
    }

    // BEK setup
    // Compute binary files hash
    QString appPath = QCoreApplication::applicationFilePath();
    QByteArray binaryHash = BrowserExamKey::computeBinaryFilesHash(appPath);
    m_examKey->setBinaryFilesHash(binaryHash);

    // Set decrypted plist XML for BEK computation
    m_examKey->setConfigPlistXml(config->decryptedPlistXml());

    // examKeySalt extracted from parsed .seb plist
    m_examKey->setExamKeySalt(config->examConfig().examKeySalt);

    // Config Key setup
    m_configKey->setConfigData(config->rawConfigData());
    // Pass parsed settings map for proper SEB-JSON generation
    m_configKey->setSettingsMap(config->settingsMap());

    qInfo() << "SEB protocol initialized";
    qInfo() << "Binary hash:" << binaryHash.toHex().left(16) << "...";

    return true;
}

QByteArray SEBProtocol::computeRequestHash(const QUrl& requestUrl) const
{
    return m_examKey->computeRequestHash(requestUrl);
}

QByteArray SEBProtocol::computeConfigKeyHash(const QUrl& requestUrl) const
{
    return m_configKey->computeRequestHash(requestUrl);
}

QString SEBProtocol::requestHashHeaderName()
{
    return QStringLiteral("X-SafeExamBrowser-RequestHash");
}

QString SEBProtocol::configKeyHeaderName()
{
    return QStringLiteral("X-SafeExamBrowser-ConfigKeyHash");
}

QString SEBProtocol::sebUserAgent()
{
    return QStringLiteral("SEB/3.0 OpenLock/0.1.0");
}

} // namespace openlock
