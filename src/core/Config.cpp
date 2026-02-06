// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "core/Config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QDebug>

namespace openlock {

Config::Config(QObject* parent)
    : QObject(parent)
{
}

Config::~Config() = default;

bool Config::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit configError("Cannot open config file: " + path);
        return false;
    }

    QByteArray data = file.readAll();
    m_rawData = data;

    if (isSebFile(path)) {
        m_format = ConfigFormat::SEB;
        return parseSebConfig(data);
    } else {
        m_format = ConfigFormat::OpenLock;
        return parseOpenLockConfig(data);
    }
}

bool Config::loadFromUrl(const QUrl& url)
{
    // Network loading will be implemented with QNetworkAccessManager
    Q_UNUSED(url);
    emit configError("URL config loading not yet implemented");
    return false;
}

bool Config::loadFromSebData(const QByteArray& data, const QString& password)
{
    m_format = ConfigFormat::SEB;
    m_rawData = data;

    QByteArray configData = data;
    if (!password.isEmpty()) {
        configData = decryptSebConfig(data, password);
        if (configData.isEmpty()) {
            emit configError("Failed to decrypt SEB config");
            return false;
        }
    }

    return parseSebConfig(configData);
}

ConfigFormat Config::format() const { return m_format; }
const ExamConfig& Config::examConfig() const { return m_examConfig; }

QByteArray Config::rawConfigData() const { return m_rawData; }

QByteArray Config::configKeyHash() const
{
    return QCryptographicHash::hash(m_rawData, QCryptographicHash::Sha256);
}

bool Config::isSebFile(const QString& path)
{
    return path.endsWith(".seb", Qt::CaseInsensitive);
}

bool Config::isOpenLockFile(const QString& path)
{
    return path.endsWith(".openlock", Qt::CaseInsensitive);
}

bool Config::parseOpenLockConfig(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        emit configError("JSON parse error: " + error.errorString());
        return false;
    }

    QJsonObject root = doc.object();

    // General
    m_examConfig.examName = root["examName"].toString();
    m_examConfig.startUrl = QUrl(root["startUrl"].toString());
    m_examConfig.exitPassword = root["exitPassword"].toString();
    m_examConfig.allowQuit = root["allowQuit"].toBool(false);

    // Navigation
    QJsonObject nav = root["navigation"].toObject();
    for (const auto& v : nav["allowedUrlPatterns"].toArray())
        m_examConfig.allowedUrlPatterns.append(v.toString());
    for (const auto& v : nav["blockedUrlPatterns"].toArray())
        m_examConfig.blockedUrlPatterns.append(v.toString());
    m_examConfig.allowNavigation = nav["allowNavigation"].toBool(true);
    m_examConfig.allowReload = nav["allowReload"].toBool(true);
    m_examConfig.allowBackForward = nav["allowBackForward"].toBool(false);

    // Browser
    QJsonObject browser = root["browser"].toObject();
    m_examConfig.userAgent = browser["userAgent"].toString();
    m_examConfig.enableJavaScript = browser["enableJavaScript"].toBool(true);
    m_examConfig.allowDownloads = browser["allowDownloads"].toBool(false);
    m_examConfig.allowPrint = browser["allowPrint"].toBool(false);
    m_examConfig.allowClipboard = browser["allowClipboard"].toBool(false);
    m_examConfig.showToolbar = browser["showToolbar"].toBool(true);

    // Security
    QJsonObject security = root["security"].toObject();
    m_examConfig.detectVM = security["detectVM"].toBool(true);
    m_examConfig.detectDebugger = security["detectDebugger"].toBool(true);
    m_examConfig.allowScreenCapture = security["allowScreenCapture"].toBool(false);
    for (const auto& v : security["processBlocklist"].toArray())
        m_examConfig.processBlocklist.append(v.toString());

    // Kiosk
    QJsonObject kiosk = root["kiosk"].toObject();
    m_examConfig.fullscreen = kiosk["fullscreen"].toBool(true);
    m_examConfig.multiMonitorLockdown = kiosk["multiMonitorLockdown"].toBool(true);
    m_examConfig.blockTaskSwitching = kiosk["blockTaskSwitching"].toBool(true);

    // SSO
    QJsonObject network = root["network"].toObject();
    for (const auto& v : network["ssoAllowedDomains"].toArray())
        m_examConfig.ssoAllowedDomains.append(v.toString());
    m_examConfig.allowWebRTC = network["allowWebRTC"].toBool(false);

    emit configLoaded();
    return true;
}

bool Config::parseSebConfig(const QByteArray& data)
{
    // SEB config files are XML plist format (Apple-style property lists)
    // They may be prefixed with a 4-byte header indicating compression/encryption
    m_examConfig.sebMode = true;
    m_examConfig.sebConfigData = data;

    QByteArray xmlData = data;

    // Check for SEB config prefix bytes
    // Prefix 'pswd' (0x70737764) = password encrypted
    // Prefix 'phsk' (0x7068736B) = public key hash encrypted
    // Prefix 'plnd' (0x706C6E64) = plaintext, compressed
    // No prefix / starts with '<' = plain XML
    if (xmlData.size() > 4) {
        QByteArray prefix = xmlData.left(4);
        if (prefix == "pswd" || prefix == "phsk") {
            if (m_examConfig.sebConfigPassword.isEmpty()) {
                emit configError("SEB config is encrypted. Password required.");
                return false;
            }
            xmlData = decryptSebConfig(xmlData, m_examConfig.sebConfigPassword);
            if (xmlData.isEmpty()) return false;
        } else if (prefix == "plnd") {
            // Compressed: skip prefix, decompress with zlib
            xmlData = qUncompress(xmlData.mid(4));
            if (xmlData.isEmpty()) {
                emit configError("Failed to decompress SEB config");
                return false;
            }
        }
    }

    // Parse XML plist
    QXmlStreamReader xml(xmlData);

    // Navigate to the root dict
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("dict")) {
            break;
        }
    }

    // Parse key-value pairs from the plist dict
    QString currentKey;
    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("key")) {
                currentKey = xml.readElementText();
            } else if (xml.name() == QStringLiteral("string")) {
                QString value = xml.readElementText();
                if (currentKey == "startURL") {
                    m_examConfig.startUrl = QUrl(value);
                } else if (currentKey == "hashedQuitPassword") {
                    m_examConfig.exitPassword = value;
                } else if (currentKey == "browserUserAgent") {
                    m_examConfig.userAgent = value;
                }
            } else if (xml.name() == QStringLiteral("true")) {
                if (currentKey == "allowQuit") m_examConfig.allowQuit = true;
                else if (currentKey == "enableJavaScript") m_examConfig.enableJavaScript = true;
                else if (currentKey == "allowDownloads") m_examConfig.allowDownloads = true;
                else if (currentKey == "enablePrinting") m_examConfig.allowPrint = true;
                else if (currentKey == "allowBrowsingBackForward") m_examConfig.allowBackForward = true;
                else if (currentKey == "enableClipboard") m_examConfig.allowClipboard = true;
            } else if (xml.name() == QStringLiteral("false")) {
                if (currentKey == "allowQuit") m_examConfig.allowQuit = false;
                else if (currentKey == "enableJavaScript") m_examConfig.enableJavaScript = false;
                else if (currentKey == "allowDownloads") m_examConfig.allowDownloads = false;
                else if (currentKey == "enablePrinting") m_examConfig.allowPrint = false;
                else if (currentKey == "allowBrowsingBackForward") m_examConfig.allowBackForward = false;
                else if (currentKey == "enableClipboard") m_examConfig.allowClipboard = false;
            } else if (xml.name() == QStringLiteral("integer")) {
                // Handle integer config values as needed
                xml.readElementText();
            }
        }
    }

    if (xml.hasError()) {
        emit configError("XML parse error: " + xml.errorString());
        return false;
    }

    emit configLoaded();
    return true;
}

QByteArray Config::decryptSebConfig(const QByteArray& encrypted, const QString& password)
{
    // SEB uses AES-256-CBC with PBKDF2 key derivation
    // Implementation requires OpenSSL - full implementation in Phase 4
    Q_UNUSED(encrypted);
    Q_UNUSED(password);
    qWarning() << "SEB config decryption not yet fully implemented";
    return {};
}

} // namespace openlock
