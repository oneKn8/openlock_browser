// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "core/LockdownEngine.h"
#include "core/Config.h"
#include "browser/SecureBrowser.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QScreen>
#include <QUrl>
#include <QDebug>

#include <cstdio>
#include <libintl.h>
#include "localepath.h"

using namespace openlock;

namespace {

// What a positional argument turned out to be. The desktop entry passes either a
// seb:// / sebs:// link (x-scheme-handler) or a .seb file (application/seb) as %u.
struct LaunchTarget {
    QString configPath;  // local .seb / .openlock file
    QUrl startUrl;       // web URL to open (a seb(s):// link mapped to http(s))
    QString error;
};

LaunchTarget classifyPositional(const QString& arg)
{
    LaunchTarget target;
    const QUrl url(arg, QUrl::StrictMode);

    // SEB spec: seb:// is the http variant, sebs:// the https variant of the same URL.
    if (url.isValid() && (url.scheme() == QLatin1String("seb") || url.scheme() == QLatin1String("sebs"))) {
        QUrl web(url);
        web.setScheme(url.scheme() == QLatin1String("sebs") ? QStringLiteral("https")
                                                             : QStringLiteral("http"));
        if (web.host().isEmpty()) {
            target.error = QStringLiteral("SEB link has no host: %1").arg(arg);
        } else {
            target.startUrl = web;
        }
        return target;
    }

    const QString path = url.isLocalFile() ? url.toLocalFile() : arg;
    const QFileInfo info(path);
    if (info.isFile()) {
        target.configPath = info.absoluteFilePath();
        return target;
    }

    target.error = QStringLiteral("Not a seb:// or sebs:// link or an existing config file: %1").arg(arg);
    return target;
}

int usageError(const QCommandLineParser& parser, const QString& message)
{
    std::fprintf(stderr, "openlock: %s\n\n%s", qUtf8Printable(message), qUtf8Printable(parser.helpText()));
    return 2;
}

} // namespace

int main(int argc, char* argv[])
{
    bindtextdomain("OpenLock", translationDirectory().toUtf8().data());
    textdomain("OpenLock");

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

#ifdef OPENLOCK_DEVELOPER_OPTIONS
    // Only in developer builds. A release binary must not offer a switch that drops the
    // lockdown while still sending valid SEB keys: the LMS cannot tell the difference.
    QCommandLineOption noLockdownOption(
        "no-lockdown",
        "Disable lockdown features (developer builds only)"
    );
    parser.addOption(noLockdownOption);

    QCommandLineOption noVmCheckOption(
        "no-vm-check",
        "Disable VM detection (developer builds only)"
    );
    parser.addOption(noVmCheckOption);
#endif

    parser.addPositionalArgument("target",
        "seb:// or sebs:// link, or a .seb / .openlock file", "[target]");

    parser.process(app);

    QString configPath = parser.value(configOption);
    QUrl startUrl;
    if (parser.isSet(urlOption)) {
        startUrl = QUrl(parser.value(urlOption), QUrl::StrictMode);
        if (!startUrl.isValid()) {
            return usageError(parser, QStringLiteral("Invalid --url: %1").arg(parser.value(urlOption)));
        }
    }

    const QStringList positionalArgs = parser.positionalArguments();
    if (positionalArgs.size() > 1) {
        return usageError(parser, QStringLiteral("Expected at most one target, got %1").arg(positionalArgs.size()));
    }
    if (!positionalArgs.isEmpty()) {
        const LaunchTarget target = classifyPositional(positionalArgs.first());
        if (!target.error.isEmpty()) {
            return usageError(parser, target.error);
        }
        if (!target.configPath.isEmpty()) {
            if (!configPath.isEmpty()) {
                return usageError(parser, QStringLiteral("Give the config either with --config or as the target, not both"));
            }
            configPath = target.configPath;
        }
        if (target.startUrl.isValid()) {
            if (startUrl.isValid()) {
                return usageError(parser, QStringLiteral("Give the start URL either with --url or as a seb:// link, not both"));
            }
            startUrl = target.startUrl;
        }
    }

    auto engine = std::make_unique<LockdownEngine>();

    if (!engine->initialize(configPath)) {
        qCritical() << "Failed to load configuration:" << (configPath.isEmpty() ? QStringLiteral("<default>") : configPath);
        return 1;
    }

    bool devMode = false;
#ifdef OPENLOCK_DEVELOPER_OPTIONS
    if (parser.isSet(noVmCheckOption)) {
        qWarning() << "*** VM DETECTION DISABLED - DEVELOPER BUILD ***";
        engine->setVmDetectionEnabled(false);
    }
    devMode = parser.isSet(noLockdownOption);
    if (devMode) {
        qWarning() << "*** LOCKDOWN DISABLED - DEVELOPER BUILD ***";
    }
#endif

    if (!devMode) {
        if (!engine->engageLockdown()) {
            qCritical() << "Failed to engage lockdown";
            return 1;
        }
    }

    SecureBrowser* browser = engine->browser();
    if (!browser) {
        qCritical() << "Browser was not created";
        return 1;
    }
    if (startUrl.isValid()) {
        browser->navigateTo(startUrl);
    } else if (browser->currentUrl().isEmpty() || !browser->currentUrl().isValid()) {
        // An exam browser with nowhere to go is a misconfiguration; never fall back to the
        // open web.
        qCritical() << "No start URL: pass a seb:// link, --url, or a config with a start URL";
        if (!devMode) {
            engine->releaseLockdown();
        }
        return 2;
    }
    if (engine->config()->examConfig().fullscreen) {
        // Size to the screen explicitly as well: in a bare X session with no window
        // manager, nothing honors the fullscreen request and the window stays tiny.
        if (QScreen* screen = browser->screen()) {
            browser->setGeometry(screen->geometry());
        }
        browser->showFullScreen();
    } else {
        browser->show();
    }

    int result = app.exec();

    if (!devMode) {
        engine->releaseLockdown();
    }

    return result;
}
