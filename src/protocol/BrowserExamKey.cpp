// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "protocol/BrowserExamKey.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

namespace openlock {

BrowserExamKey::BrowserExamKey() = default;
BrowserExamKey::~BrowserExamKey() = default;

void BrowserExamKey::setExamKeySalt(const QByteArray& salt) { m_examKeySalt = salt; }
void BrowserExamKey::setConfigPlistXml(const QByteArray& xml) { m_configPlistXml = xml; }
void BrowserExamKey::setBinaryFilesHash(const QByteArray& hash) { m_binaryFilesHash = hash; }

QByteArray BrowserExamKey::computeRawKey() const
{
    // SEB BEK algorithm (from SEBProtectionController.cs):
    // 1. message = UTF8(XMLPlist(settings) + SHA256_hex(binary_files))
    // 2. BEK = HMAC-SHA256(key=examKeySalt, message)

    if (m_examKeySalt.isEmpty()) {
        qWarning() << "BEK: examKeySalt is empty, using zero salt";
    }

    // Build the message: config XML + binary hash hex string
    QByteArray message = m_configPlistXml;
    if (!m_binaryFilesHash.isEmpty()) {
        message.append(m_binaryFilesHash.toHex());
    }

    // HMAC-SHA256 with examKeySalt as key
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, m_examKeySalt);
    hmac.addData(message);
    return hmac.result();
}

QByteArray BrowserExamKey::computeRequestHash(const QUrl& requestUrl) const
{
    // Per-request header value (from SEBBrowserController.m):
    // 1. str = url_without_fragment + hex(rawBEK)
    // 2. header = hex(SHA256(UTF8(str)))

    QByteArray rawBek = computeRawKey();
    QUrl cleanUrl = stripFragment(requestUrl);

    QByteArray combined;
    combined.append(cleanUrl.toString().toUtf8());
    combined.append(rawBek.toHex());

    QByteArray hash = QCryptographicHash::hash(combined, QCryptographicHash::Sha256);
    return hash.toHex();
}

QByteArray BrowserExamKey::computeBinaryFilesHash(const QString& appPath)
{
    // Hash all SEB/OpenLock binary files:
    // 1. For each file: SHA256(contents) -> hex string
    // 2. Concatenate all hex strings
    // 3. SHA256(concatenated) -> final hash

    QFileInfo appInfo(appPath);
    QDir appDir = appInfo.dir();

    QStringList binaryFiles;
    // Add the main executable
    binaryFiles << appPath;

    // Add all .so files in the same directory
    QStringList filters;
    filters << "*.so" << "*.so.*";
    QFileInfoList libs = appDir.entryInfoList(filters, QDir::Files);
    for (const auto& lib : libs) {
        binaryFiles << lib.absoluteFilePath();
    }

    binaryFiles.sort();

    QByteArray allHashes;
    for (const QString& path : binaryFiles) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;

        QByteArray fileHash = QCryptographicHash::hash(
            file.readAll(), QCryptographicHash::Sha256);
        allHashes.append(fileHash.toHex());
    }

    return QCryptographicHash::hash(allHashes, QCryptographicHash::Sha256);
}

QUrl BrowserExamKey::stripFragment(const QUrl& url)
{
    QUrl clean = url;
    clean.setFragment(QString());
    return clean;
}

} // namespace openlock
