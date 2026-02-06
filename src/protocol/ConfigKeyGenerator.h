// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QByteArray>
#include <QUrl>
#include <QVariantMap>

namespace openlock {

class ConfigKeyGenerator {
public:
    ConfigKeyGenerator();
    ~ConfigKeyGenerator();

    void setConfigData(const QByteArray& data);
    void setSettingsMap(const QVariantMap& settings);

    // Compute the raw Config Key (32 bytes)
    // ConfigKey = SHA256(UTF8(SEB_JSON_string))
    // SEB-JSON: sorted keys, no whitespace, no originatorVersion
    QByteArray computeRawKey() const;

    // Compute the per-request header value
    // header = hex(SHA256(UTF8(url_no_fragment + hex(rawConfigKey))))
    QByteArray computeRequestHash(const QUrl& requestUrl) const;

private:
    QString settingsToSebJson(const QVariantMap& settings) const;
    QString variantToJson(const QVariant& value) const;
    static QUrl stripFragment(const QUrl& url);

    QByteArray m_configData;
    QVariantMap m_settingsMap;
};

} // namespace openlock
