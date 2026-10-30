// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QByteArray>
#include <QString>

namespace openlock {

class SEBConfigParser {
public:
    // Decrypt and decompress a .seb file, returning raw XML plist bytes
    // Handles: outer gzip, pswd/pwcc prefix (RNCryptor v3), plnd prefix, inner gzip, and raw XML
    // Returns empty QByteArray on failure
    static QByteArray decryptSebFile(const QByteArray& data, const QString& password);

    // RNCryptor v3 decryption (exposed for testing).
    static QByteArray decryptRNCryptorV3(const QByteArray& data, const QString& password);

private:
    static QByteArray tryGzipDecompress(const QByteArray& data);
};

} // namespace openlock