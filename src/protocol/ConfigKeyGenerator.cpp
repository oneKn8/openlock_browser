// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "protocol/ConfigKeyGenerator.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QDebug>

#include <algorithm>

namespace openlock {

namespace {

    QString jsonEscape(const QString& s)
    {
        QString out;
        out.reserve(s.size());
        for (const QChar c : s) {
            const ushort u = c.unicode();
            switch (u) {
            case '"':  out += QLatin1String("\\\""); break;
            case '\b': out += QLatin1String("\\b");  break;
            case '\f': out += QLatin1String("\\f");  break;
            case '\n': out += QLatin1String("\\n");  break;
            case '\r': out += QLatin1String("\\r");  break;
            case '\t': out += QLatin1String("\\t");  break;
            default:
                if (u < 0x20 || u == 0x2028 || u == 0x2029)
                    out += QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
                else
                    out += c;
            }
        }
        return out;
    }

}

ConfigKeyGenerator::ConfigKeyGenerator() = default;
ConfigKeyGenerator::~ConfigKeyGenerator() = default;

void ConfigKeyGenerator::setConfigData(const QByteArray& data) { m_configData = data; }
void ConfigKeyGenerator::setSettingsMap(const QVariantMap& settings) { m_settingsMap = settings; }

QByteArray ConfigKeyGenerator::computeRawKey() const
{
    // Config Key algorithm (from SEB docs):
    // 1. Convert settings to SEB-JSON format:
    //    - Remove "originatorVersion" key
    //    - Sort all dictionary keys alphabetically (case-insensitive)
    //    - No whitespace, no line breaks
    //    - Recursively sort nested dicts
    // 2. ConfigKey = SHA256(UTF8(SEB_JSON_string))

    if (m_settingsMap.isEmpty()) {
        // Fallback: hash raw config data if no parsed settings map available
        return QCryptographicHash::hash(m_configData, QCryptographicHash::Sha256);
    }

    // Build SEB-JSON from settings
    QVariantMap cleanSettings = m_settingsMap;
    cleanSettings.remove("originatorVersion");

    QString json = settingsToSebJson(cleanSettings);
    return QCryptographicHash::hash(json.toUtf8(), QCryptographicHash::Sha256);
}

QByteArray ConfigKeyGenerator::computeRequestHash(const QUrl& requestUrl) const
{
    // Per-request header (same pattern as BEK):
    // header = hex(SHA256(UTF8(url_no_fragment + hex(rawConfigKey))))

    QByteArray rawKey = computeRawKey();
    QUrl cleanUrl = stripFragment(requestUrl);

    QByteArray combined;
    combined.append(cleanUrl.toString().toUtf8());
    combined.append(rawKey.toHex());

    QByteArray hash = QCryptographicHash::hash(combined, QCryptographicHash::Sha256);
    return hash.toHex();
}

QString ConfigKeyGenerator::settingsToSebJson(const QVariantMap& settings) const
{
    // SEB-JSON format:
    // - Dictionary with sorted keys (case-insensitive)
    // - No whitespace
    // - No character escaping beyond what's necessary
    // - Data -> Base64, Date -> ISO 8601, booleans -> true/false

    QString result = "{";
    QStringList keys = settings.keys();

    // Sort case-insensitive (culture-invariant)
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {
        int cmp = a.compare(b, Qt::CaseInsensitive);
        if (cmp != 0) return cmp < 0;
        return a < b;  // lowercase-first tie-break
    });

    bool first = true;
    for (const QString& key : keys) {
        QVariant v = settings[key];
        if (v.typeId() == QMetaType::QVariantMap && v.toMap().isEmpty())
            continue;

        if (!first) result += ",";
        first = false;

        result += "\"" + jsonEscape(key) + "\":";
        result += variantToJson(v);
    }

    result += "}";
    return result;
}

QString ConfigKeyGenerator::variantToJson(const QVariant& value) const
{
    switch (value.typeId()) {
    case QMetaType::Bool:
        return value.toBool() ? "true" : "false";

    case QMetaType::Int:
    case QMetaType::LongLong:
        return QString::number(value.toLongLong());

    case QMetaType::Double:
    case QMetaType::Float: {
        double d = value.toDouble();
        if (std::isfinite(d) && d == std::floor(d) && std::abs(d) < 1e15)
            return QString::number(static_cast<long long>(d));
        return QString::number(d, 'g', 15);
    }

    case QMetaType::QString:
        return "\"" + jsonEscape(value.toString()) + "\"";

    case QMetaType::QByteArray:
        // Data -> Base64
        return "\"" + value.toByteArray().toBase64() + "\"";

    case QMetaType::QDateTime: {
        QString s = value.toDateTime().toString(Qt::ISODate);
        if (s.endsWith('Z'))
            s = s.chopped(1) + "+00:00";
        return "\"" + s + "\"";
    }

    case QMetaType::QVariantMap: {
        return settingsToSebJson(value.toMap());
    }

    case QMetaType::QVariantList: {
        QVariantList list = value.toList();
        QString result = "[";
        for (int i = 0; i < list.size(); i++) {
            if (i > 0) result += ",";
            result += variantToJson(list[i]);
        }
        result += "]";
        return result;
    }

    default:
        return "\"" + jsonEscape(value.toString()) + "\"";
    }
}

QUrl ConfigKeyGenerator::stripFragment(const QUrl& url)
{
    QUrl clean = url;
    clean.setFragment(QString());
    return clean;
}

} // namespace openlock
