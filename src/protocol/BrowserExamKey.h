// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QByteArray>
#include <QUrl>

namespace openlock {

class BrowserExamKey {
public:
    BrowserExamKey();
    ~BrowserExamKey();

    void setExamKeySalt(const QByteArray& salt);
    void setConfigPlistXml(const QByteArray& xml);
    void setBinaryFilesHash(const QByteArray& hash);

    // Compute the raw BEK (32 bytes)
    // BEK = HMAC-SHA256(key=examKeySalt, msg=UTF8(configXml + binaryHashHex))
    QByteArray computeRawKey() const;

    // Compute the per-request header value
    // header = hex(SHA256(UTF8(url_no_fragment + hex(rawBEK))))
    QByteArray computeRequestHash(const QUrl& requestUrl) const;

    static QByteArray computeBinaryFilesHash(const QString& appPath);

private:
    static QUrl stripFragment(const QUrl& url);

    QByteArray m_examKeySalt;        // 32-byte random salt from .seb config
    QByteArray m_configPlistXml;     // XML plist of current settings
    QByteArray m_binaryFilesHash;    // SHA-256 of concatenated file hashes
};

} // namespace openlock
