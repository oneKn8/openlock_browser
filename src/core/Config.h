// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QJsonObject>

namespace openlock {

enum class ConfigFormat {
    OpenLock,   // Native JSON format (.openlock)
    SEB         // Safe Exam Browser plist format (.seb)
};

struct ExamConfig {
    // General
    QString examName;
    QUrl startUrl;
    QString exitPassword;
    bool allowQuit = false;

    // Navigation
    QStringList allowedUrlPatterns;
    QStringList blockedUrlPatterns;
    bool allowNavigation = true;
    bool allowReload = true;
    bool allowBackForward = false;

    // Browser
    QString userAgent;
    bool enableJavaScript = true;
    bool enablePlugins = false;
    bool allowDownloads = false;
    bool allowPrint = false;
    bool allowClipboard = false;
    bool showToolbar = true;

    // Security
    bool detectVM = true;
    bool detectDebugger = true;
    bool allowScreenCapture = false;
    QStringList processBlocklist;
    QStringList additionalAllowedProcesses;

    // SEB-specific
    bool sebMode = false;
    QByteArray sebConfigData;    // Raw config for Config Key computation
    QString sebConfigPassword;

    // Kiosk
    bool fullscreen = true;
    bool multiMonitorLockdown = true;
    bool blockTaskSwitching = true;

    // Network
    QStringList ssoAllowedDomains;
    bool allowWebRTC = false;
};

class Config : public QObject {
    Q_OBJECT

public:
    explicit Config(QObject* parent = nullptr);
    ~Config() override;

    bool loadFromFile(const QString& path);
    bool loadFromUrl(const QUrl& url);
    bool loadFromSebData(const QByteArray& data, const QString& password = {});

    ConfigFormat format() const;
    const ExamConfig& examConfig() const;

    QByteArray rawConfigData() const;
    QByteArray configKeyHash() const;

    static bool isSebFile(const QString& path);
    static bool isOpenLockFile(const QString& path);

signals:
    void configLoaded();
    void configError(const QString& message);

private:
    bool parseOpenLockConfig(const QByteArray& data);
    bool parseSebConfig(const QByteArray& data);
    QByteArray decryptSebConfig(const QByteArray& encrypted, const QString& password);

    ConfigFormat m_format = ConfigFormat::OpenLock;
    ExamConfig m_examConfig;
    QByteArray m_rawData;
};

} // namespace openlock
