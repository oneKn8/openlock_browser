// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "core/Config.h"
#include "protocol/SEBConfigParser.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QInputDialog>

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

        // Detect if password-encrypted (peek at prefix after outer gzip)
        QByteArray working = data;
        {
            // reuse decryptSebFile's gzip logic by just peeking — try decompress
            QByteArray peek = SEBConfigParser::tryOuterGzip(data);
            if (!peek.isEmpty()) working = peek;
        }
        bool needsPassword = working.size() >= 4 &&
                            (working.left(4) == "pswd" || working.left(4) == "pwcc");

        QByteArray xmlData;
        if (needsPassword) {
            for (int attempt = 0; attempt < 5; ++attempt) {
                bool ok = false;
                QString pw = QInputDialog::getText(
                    nullptr,
                    "SEB Password",
                    attempt == 0 ? "Enter the SEB config password:"
                                : "Wrong password, try again:",
                    QLineEdit::Password, {}, &ok);
                if (!ok) break;
                m_examConfig.sebConfigPassword = pw;
                xmlData = SEBConfigParser::decryptSebFile(data, pw);
                if (!xmlData.isEmpty()) break;
            }
        } else {
            xmlData = SEBConfigParser::decryptSebFile(data, {});
        }

        if (xmlData.isEmpty()) {
            emit configError("Failed to decrypt/decompress SEB config: " + path);
            return false;
        }
        return parseSebConfig(xmlData);
    }
    m_format = ConfigFormat::OpenLock;
    return parseOpenLockConfig(data);
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

    QByteArray xmlData = SEBConfigParser::decryptSebFile(data, password);
    if (xmlData.isEmpty()) {
        emit configError("Failed to decrypt SEB config");
        return false;
    }
    return parseSebConfig(xmlData);
}

ConfigFormat Config::format() const { return m_format; }
const ExamConfig& Config::examConfig() const { return m_examConfig; }

QByteArray Config::rawConfigData() const { return m_rawData; }

QByteArray Config::configKeyHash() const
{
    return QCryptographicHash::hash(m_rawData, QCryptographicHash::Sha256);
}

QVariantMap Config::settingsMap() const { return m_examConfig.settingsMap; }

QByteArray Config::decryptedPlistXml() const { return m_decryptedPlistXml; }

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

static QVariant parsePlistValue(QXmlStreamReader& xml);
static QVariantMap parsePlistDict(QXmlStreamReader& xml);
static QVariantList parsePlistArray(QXmlStreamReader& xml);

static QVariantMap parsePlistDict(QXmlStreamReader& xml)
{
    QVariantMap map;
    QString currentKey;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement() && xml.name() == QStringLiteral("dict"))
            break;
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QStringLiteral("key")) {
            currentKey = xml.readElementText();
        } else if (!currentKey.isEmpty()) {
            QVariant value = parsePlistValue(xml);
            if (!(value.typeId() == QMetaType::QVariantMap && value.toMap().isEmpty()))
                map.insert(currentKey, value);
            currentKey.clear();
        }
    }
    return map;
}

static QVariantList parsePlistArray(QXmlStreamReader& xml)
{
    QVariantList list;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement() && xml.name() == QStringLiteral("array"))
            break;
        if (!xml.isStartElement())
            continue;
        QVariant value = parsePlistValue(xml);
        if (!(value.typeId() == QMetaType::QVariantMap && value.toMap().isEmpty()))
            list.append(value);
    }
    return list;
}

static QVariant parsePlistValue(QXmlStreamReader& xml)
{
    const QString elemName = xml.name().toString();
    if (elemName == QStringLiteral("string"))
        return xml.readElementText();
    if (elemName == QStringLiteral("true")) { xml.readElementText(); return true; }
    if (elemName == QStringLiteral("false")) { xml.readElementText(); return false; }
    if (elemName == QStringLiteral("integer"))
        return xml.readElementText().toLongLong();
    if (elemName == QStringLiteral("real"))
        return xml.readElementText().toDouble();
    if (elemName == QStringLiteral("data"))
        return QByteArray::fromBase64(xml.readElementText().toUtf8());
    if (elemName == QStringLiteral("date"))
        return QDateTime::fromString(xml.readElementText(), Qt::ISODate);
    if (elemName == QStringLiteral("dict"))
        return parsePlistDict(xml);
    if (elemName == QStringLiteral("array"))
        return parsePlistArray(xml);
    xml.readElementText();
    return {};
}

bool Config::parseSebConfig(const QByteArray& data)
{
    // SEB config files are XML plist format (Apple-style property lists)
    m_examConfig.sebMode = true;
    m_examConfig.sebConfigData = data;
    m_decryptedPlistXml = data;

    // Parse XML plist
    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("dict"))
            break;
    }
    if (xml.atEnd() || xml.hasError()) {
        emit configError("SEB plist: no root dict element found");
        return false;
    }
    m_examConfig.settingsMap = parsePlistDict(xml);
    if (xml.hasError()) {
        emit configError("XML parse error: " + xml.errorString());
        return false;
    }
    if (m_examConfig.settingsMap.contains("examKeySalt"))
        m_examConfig.examKeySalt = m_examConfig.settingsMap.value("examKeySalt").toByteArray();
    m_examConfig.startUrl         = QUrl(m_examConfig.settingsMap.value("startURL").toString());
    m_examConfig.exitPassword     = m_examConfig.settingsMap.value("hashedQuitPassword").toString();
    m_examConfig.userAgent        = m_examConfig.settingsMap.value("browserUserAgent").toString();
    m_examConfig.allowQuit        = m_examConfig.settingsMap.value("allowQuit", false).toBool();
    m_examConfig.enableJavaScript = m_examConfig.settingsMap.value("enableJavaScript", true).toBool();
    m_examConfig.allowDownloads   = m_examConfig.settingsMap.value("allowDownloads", false).toBool();
    m_examConfig.allowPrint       = m_examConfig.settingsMap.value("enablePrinting", false).toBool();
    m_examConfig.allowBackForward = m_examConfig.settingsMap.value("allowBrowsingBackForward", false).toBool();
    m_examConfig.allowClipboard   = m_examConfig.settingsMap.value("enableClipboard", false).toBool();

    emit configLoaded();
    return true;
}

QByteArray Config::decryptSebConfig(const QByteArray& encrypted, const QString& password)
{
    return SEBConfigParser::decryptSebFile(encrypted, password);
}

} // namespace openlock
