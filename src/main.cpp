// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "core/LockdownEngine.h"
#include "core/Config.h"
#include "browser/SecureBrowser.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QUrl>
#include <QDebug>

using namespace openlock;

int main(int argc, char* argv[])
{
    // QtWebEngine requires this before QApplication
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setApplicationName("OpenLock");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenLock");

    QCommandLineParser parser;
    parser.setApplicationDescription("OpenLock - Open-Source Linux Lockdown Exam Browser");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Path to configuration file (.openlock or .seb)",
        "file"
    );
    parser.addOption(configOption);

    QCommandLineOption urlOption(
        QStringList() << "u" << "url",
        "Start URL (LMS login page)",
        "url"
    );
    parser.addOption(urlOption);

    QCommandLineOption noLockdownOption(
        "no-lockdown",
        "Disable lockdown features (for development/testing only)"
    );
    parser.addOption(noLockdownOption);

    QCommandLineOption noVmCheckOption(
        "no-vm-check",
        "Disable VM detection (for testing in VMs)"
    );
    parser.addOption(noVmCheckOption);

    // Handle seb:// and sebs:// URL arguments
    parser.addPositionalArgument("seburl", "SEB URL to open (seb:// or sebs://)", "[seb-url]");

    parser.process(app);

    auto engine = std::make_unique<LockdownEngine>();

    // Load configuration
    QString configPath = parser.value(configOption);
    if (!configPath.isEmpty()) {
        if (!engine->initialize(configPath)) {
            qCritical() << "Failed to load configuration:" << configPath;
            return 1;
        }
    } else {
        // Use default config
        if (!engine->initialize({})) {
            qCritical() << "Failed to initialize with default configuration";
            return 1;
        }
    }

    // Override start URL if provided
    QUrl startUrl;
    if (parser.isSet(urlOption)) {
        startUrl = QUrl(parser.value(urlOption));
    }

    // Handle positional seb:// URL
    const auto positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty()) {
        QString sebUrl = positionalArgs.first();
        if (sebUrl.startsWith("seb://") || sebUrl.startsWith("sebs://")) {
            // Convert seb:// to https:// for loading
            sebUrl.replace("seb://", "https://");
            sebUrl.replace("sebs://", "https://");
            startUrl = QUrl(sebUrl);
        }
    }

    // Apply command-line overrides
    if (parser.isSet(noVmCheckOption)) {
        qWarning() << "VM detection disabled via command-line flag";
    }

    bool devMode = parser.isSet(noLockdownOption);
    if (devMode) {
        qWarning() << "*** LOCKDOWN DISABLED - DEVELOPMENT MODE ***";
    }

    // Engage lockdown (unless dev mode)
    if (!devMode) {
        if (!engine->engageLockdown()) {
            qCritical() << "Failed to engage lockdown";
            return 1;
        }
    }

    // Show browser
    SecureBrowser* browser = engine->browser();
    if (browser) {
        if (startUrl.isValid()) {
            browser->navigateTo(startUrl);
        }
        browser->show();
    }

    int result = app.exec();

    // Clean release
    if (!devMode) {
        engine->releaseLockdown();
    }

    return result;
}
