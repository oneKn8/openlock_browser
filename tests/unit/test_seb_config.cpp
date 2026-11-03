// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include "core/Config.h"
#include "protocol/BrowserExamKey.h"
#include "protocol/ConfigKeyGenerator.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>

using namespace openlock;

class ConfigTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // QCoreApplication needed for Qt features
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = { const_cast<char*>("test") };
            static QCoreApplication app(argc, argv);
        }
    }
};

TEST_F(ConfigTest, ParseOpenLockConfig) {
    QByteArray json = R"({
        "examName": "Test Exam",
        "startUrl": "https://moodle.example.com/quiz",
        "exitPassword": "secret123",
        "allowQuit": false,
        "navigation": {
            "allowedUrlPatterns": ["*.example.com/*"],
            "allowNavigation": true,
            "allowReload": true,
            "allowBackForward": false
        },
        "browser": {
            "enableJavaScript": true,
            "allowDownloads": false,
            "allowPrint": false,
            "allowClipboard": false,
            "showToolbar": true
        },
        "security": {
            "detectVM": true,
            "detectDebugger": true,
            "allowScreenCapture": false
        },
        "kiosk": {
            "fullscreen": true,
            "multiMonitorLockdown": true,
            "blockTaskSwitching": true
        },
        "network": {
            "ssoAllowedDomains": ["login.microsoftonline.com"],
            "allowWebRTC": false
        }
    })";

    QTemporaryFile tmpFile;
    tmpFile.setFileTemplate("XXXXXX.openlock");
    ASSERT_TRUE(tmpFile.open());
    tmpFile.write(json);
    tmpFile.close();

    Config config;
    ASSERT_TRUE(config.loadFromFile(tmpFile.fileName()));

    EXPECT_EQ(config.format(), ConfigFormat::OpenLock);

    const auto& exam = config.examConfig();
    EXPECT_EQ(exam.examName, "Test Exam");
    EXPECT_EQ(exam.startUrl.toString(), "https://moodle.example.com/quiz");
    EXPECT_EQ(exam.exitPassword, "secret123");
    EXPECT_FALSE(exam.allowQuit);
    EXPECT_TRUE(exam.fullscreen);
    EXPECT_FALSE(exam.allowDownloads);
    EXPECT_FALSE(exam.allowClipboard);
    EXPECT_TRUE(exam.detectVM);
    EXPECT_FALSE(exam.allowWebRTC);
}

TEST_F(ConfigTest, ParseSebConfig) {
    QByteArray sebXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>startURL</key>
    <string>https://moodle.example.com/quiz</string>
    <key>allowQuit</key>
    <false/>
    <key>enableJavaScript</key>
    <true/>
    <key>allowDownloads</key>
    <false/>
</dict>
</plist>)";

    QTemporaryFile tmpFile;
    tmpFile.setFileTemplate("XXXXXX.seb");
    ASSERT_TRUE(tmpFile.open());
    tmpFile.write(sebXml);
    tmpFile.close();

    Config config;
    ASSERT_TRUE(config.loadFromFile(tmpFile.fileName()));

    EXPECT_EQ(config.format(), ConfigFormat::SEB);

    const auto& exam = config.examConfig();
    EXPECT_TRUE(exam.sebMode);
    EXPECT_EQ(exam.startUrl.toString(), "https://moodle.example.com/quiz");
    EXPECT_FALSE(exam.allowQuit);
    EXPECT_TRUE(exam.enableJavaScript);
    EXPECT_FALSE(exam.allowDownloads);
}

TEST_F(ConfigTest, ConfigKeyHashComputed) {
    QByteArray json = R"({"examName": "Test"})";

    QTemporaryFile tmpFile;
    tmpFile.setFileTemplate("XXXXXX.openlock");
    ASSERT_TRUE(tmpFile.open());
    tmpFile.write(json);
    tmpFile.close();

    Config config;
    ASSERT_TRUE(config.loadFromFile(tmpFile.fileName()));

    QByteArray hash = config.configKeyHash();
    EXPECT_EQ(hash.size(), 32);  // SHA-256 = 32 bytes
}

TEST_F(ConfigTest, InvalidJsonFails) {
    QTemporaryFile tmpFile;
    tmpFile.setFileTemplate("XXXXXX.openlock");
    ASSERT_TRUE(tmpFile.open());
    tmpFile.write("not valid json{{{");
    tmpFile.close();

    Config config;
    EXPECT_FALSE(config.loadFromFile(tmpFile.fileName()));
}

TEST_F(ConfigTest, FileTypeDetection) {
    EXPECT_TRUE(Config::isSebFile("exam.seb"));
    EXPECT_TRUE(Config::isSebFile("EXAM.SEB"));
    EXPECT_FALSE(Config::isSebFile("exam.openlock"));

    EXPECT_TRUE(Config::isOpenLockFile("exam.openlock"));
    EXPECT_TRUE(Config::isOpenLockFile("EXAM.OPENLOCK"));
    EXPECT_FALSE(Config::isOpenLockFile("exam.seb"));
}

// Download a file via curl; returns empty on any error (no-network -> skip).
static QByteArray downloadFixture(const char* url) {
    QProcess proc;
    proc.start("curl", {"-sfL", "--max-time", "15", url});
    if (!proc.waitForFinished(20000) || proc.exitCode() != 0)
        return {};
    return proc.readAllStandardOutput();
}

// Moodle fixture: SEB Mac 2.1.4; verifies Config Key against Moodle's own test vector.
TEST_F(ConfigTest, ConfigKeyMoodleFixtureMac) {
    QByteArray data = downloadFixture(
        "https://raw.githubusercontent.com/moodle/moodle/main/"
        "public/mod/quiz/accessrule/seb/tests/fixtures/unencrypted_mac_001.seb");
    if (data.isEmpty())
        GTEST_SKIP() << "Moodle fixture unavailable (no network)";

    Config config;
    ASSERT_TRUE(config.loadFromSebData(data, {}));

    ConfigKeyGenerator gen;
    gen.setSettingsMap(config.examConfig().settingsMap);
    EXPECT_EQ(gen.computeRawKey().toHex(),
              "4fa9af8ec8759eb7c680752ef4ee5eaf1a860628608fccae2715d519849f9292");

    BrowserExamKey bek;
    bek.setExamKeySalt(config.examConfig().examKeySalt);
    bek.setConfigPlistXml(config.decryptedPlistXml());
    EXPECT_EQ(bek.computeRawKey().toHex(),
              "07d3da4d776c4aa007660803a95cdc7b82a375585284c042651e975dd42047c4");
}

// Moodle fixture: SEB Win 2.1.1; second independent test vector.
TEST_F(ConfigTest, ConfigKeyMoodleFixtureWin) {
    QByteArray data = downloadFixture(
        "https://raw.githubusercontent.com/moodle/moodle/main/"
        "public/mod/quiz/accessrule/seb/tests/fixtures/unencrypted_win_223.seb");
    if (data.isEmpty())
        GTEST_SKIP() << "Moodle fixture unavailable (no network)";

    Config config;
    ASSERT_TRUE(config.loadFromSebData(data, {}));

    ConfigKeyGenerator gen;
    gen.setSettingsMap(config.examConfig().settingsMap);
    EXPECT_EQ(gen.computeRawKey().toHex(),
              "2534e4e9f3188f9f9133bf7cf7b4c5d898292bbd7e8d0230f39d1176636a1431");

    BrowserExamKey bek;
    bek.setExamKeySalt(config.examConfig().examKeySalt);
    bek.setConfigPlistXml(config.decryptedPlistXml());
    EXPECT_EQ(bek.computeRawKey().toHex(),
              "f6f6fdc04c4c5019895514fcc56f1150d0b1acbb93165cdd36a7f68ea02af9f6");
}

// pswd fixture: password-encrypted .seb generated by gen_test_seb.py
TEST_F(ConfigTest, ConfigKeyPswdFixture) {
    QString fixturesDir = QString(__FILE__).left(QString(__FILE__).lastIndexOf('/')) + "/fixtures";
    QString path = fixturesDir + "/test_encrypted.seb";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        GTEST_SKIP() << "pswd fixture not found: " << path.toStdString();
    QByteArray data = f.readAll();

    Config config;
    ASSERT_TRUE(config.loadFromSebData(data, "exam-Pass_2026"));

    ConfigKeyGenerator gen;
    gen.setSettingsMap(config.examConfig().settingsMap);
    EXPECT_EQ(gen.computeRawKey().toHex(),
              "75553d960b2bc1f63ee2a5910e8bd023558d5cf0f17f743dbd55a93b7b454d92");

    BrowserExamKey bek;
    bek.setExamKeySalt(config.examConfig().examKeySalt);
    bek.setConfigPlistXml(config.decryptedPlistXml());
    // Regression value: Moodle publishes no BEK vectors; this guards against unintended changes.
    EXPECT_EQ(bek.computeRawKey().toHex(),
              "d39610954d13b91fc2c833f5a68d7a2387d8623b0489f9326ed467d11c469b9c");
}