// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "browser/SecureBrowser.h"
#include "browser/NavigationFilter.h"
#include "browser/DownloadBlocker.h"
#include "browser/DevToolsBlocker.h"
#include "core/Config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QKeyEvent>
#include <QDebug>

namespace openlock {

// --- SecurePage ---

SecurePage::SecurePage(QWebEngineProfile* profile, QObject* parent)
    : QWebEnginePage(profile, parent)
{
    // Deny all permissions (camera, mic, geolocation, notifications, etc.)
    connect(this, &QWebEnginePage::featurePermissionRequested,
            this, [this](const QUrl& origin, Feature feature) {
        setFeaturePermission(origin, feature, PermissionDeniedByUser);
        qWarning() << "Permission denied:" << feature << "for" << origin;
    });
}

bool SecurePage::acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame)
{
    Q_UNUSED(type);
    Q_UNUSED(isMainFrame);

    // Block dangerous URL schemes
    QString scheme = url.scheme().toLower();
    if (scheme == "file" || scheme == "about" || scheme == "chrome" ||
        scheme == "data" || scheme == "javascript" || scheme == "view-source" ||
        scheme == "blob" || scheme == "ftp" || scheme == "chrome-devtools") {
        qWarning() << "Blocked URL scheme:" << scheme << url.toString();
        return false;
    }

    // Only allow http/https
    if (scheme != "http" && scheme != "https") {
        qWarning() << "Blocked non-http scheme:" << scheme;
        return false;
    }

    return true;
}

QWebEnginePage* SecurePage::createWindow(WebWindowType type)
{
    Q_UNUSED(type);
    qWarning() << "Popup window blocked";
    return nullptr;
}

void SecurePage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                           const QString& message, int lineNumber,
                                           const QString& sourceID)
{
    Q_UNUSED(level);
    Q_UNUSED(message);
    Q_UNUSED(lineNumber);
    Q_UNUSED(sourceID);
}

void SecurePage::triggerAction(WebAction action, bool checked)
{
    // Block actions that could leak page content or enable debugging
    switch (action) {
    case QWebEnginePage::ViewSource:
    case QWebEnginePage::InspectElement:
    case QWebEnginePage::DownloadLinkToDisk:
    case QWebEnginePage::DownloadImageToDisk:
    case QWebEnginePage::DownloadMediaToDisk:
    case QWebEnginePage::SavePage:
        qWarning() << "Blocked WebAction:" << action;
        return;
    default:
        QWebEnginePage::triggerAction(action, checked);
    }
}

// --- SecureBrowser ---

SecureBrowser::SecureBrowser(QWidget* parent)
    : QWidget(parent)
    , m_downloadBlocker(std::make_unique<DownloadBlocker>())
    , m_devToolsBlocker(std::make_unique<DevToolsBlocker>())
{
}

SecureBrowser::~SecureBrowser()
{
    // Delete view first (it references the page but doesn't own it),
    // then delete the page before QObject destroys the profile.
    // This avoids "Release of profile requested but WebEnginePage still not deleted".
    delete m_webView;
    m_webView = nullptr;
    delete m_page;
    m_page = nullptr;
}

bool SecureBrowser::initialize(const Config* config)
{
    setupProfile();
    applyHardenedSettings();

    const auto& examConfig = config->examConfig();

    // Set custom User-Agent
    if (!examConfig.userAgent.isEmpty()) {
        m_profile->setHttpUserAgent(examConfig.userAgent);
    } else {
        QString ua = m_profile->httpUserAgent();
        ua += " SEB/3.0 OpenLock/0.1.0";
        m_profile->setHttpUserAgent(ua);
    }

    // Create secure page
    m_page = new SecurePage(m_profile, this);
    m_webView = new QWebEngineView(this);
    m_webView->setPage(m_page);

    // Disable right-click context menu
    m_webView->setContextMenuPolicy(Qt::NoContextMenu);

    // Inject console neutering script
    injectConsoleBlocker();

    // Install keyboard event filter on the render widget (focusProxy)
    // This catches keys before Chromium processes them
    if (m_webView->focusProxy()) {
        m_webView->focusProxy()->installEventFilter(this);
    }

    // Connect signals
    connect(m_page, &QWebEnginePage::urlChanged, this, &SecureBrowser::urlChanged);
    connect(m_page, &QWebEnginePage::loadStarted, this, &SecureBrowser::pageLoadStarted);
    connect(m_page, &QWebEnginePage::loadFinished, this, &SecureBrowser::pageLoadFinished);

    // Re-install event filter when focus proxy changes (can happen on page load)
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool) {
        if (m_webView->focusProxy()) {
            m_webView->focusProxy()->installEventFilter(this);
        }
    });

    // Layout
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    if (examConfig.showToolbar) {
        setupToolbar();
        layout->addWidget(m_toolbar);
    }

    layout->addWidget(m_webView);

    // Navigate to start URL if configured
    if (examConfig.startUrl.isValid()) {
        navigateTo(examConfig.startUrl);
    }

    return true;
}

void SecureBrowser::navigateTo(const QUrl& url)
{
    if (m_webView) {
        m_webView->setUrl(url);
    }
}

QUrl SecureBrowser::currentUrl() const
{
    return m_page ? m_page->url() : QUrl();
}

void SecureBrowser::setNavigationFilter(NavigationFilter* filter)
{
    m_navFilter = filter;
}

NavigationFilter* SecureBrowser::navigationFilter() const
{
    return m_navFilter;
}

QWebEngineView* SecureBrowser::webView() const
{
    return m_webView;
}

bool SecureBrowser::eventFilter(QObject* obj, QEvent* event)
{
    // Keyboard event filter installed on webView->focusProxy()
    // Catches keys at the render widget level before Chromium sees them
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers mods = keyEvent->modifiers();
        int key = keyEvent->key();

        // Block F12 (DevTools)
        if (key == Qt::Key_F12) return true;

        // Block Ctrl+Shift+I (DevTools)
        if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier) && key == Qt::Key_I)
            return true;

        // Block Ctrl+Shift+J (Console)
        if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier) && key == Qt::Key_J)
            return true;

        // Block Ctrl+U (View Source)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_U) return true;

        // Block Ctrl+S (Save Page)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_S) return true;

        // Block Ctrl+P (Print)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_P) return true;

        // Block Ctrl+G, Ctrl+F5 (various debug)
        if ((mods & Qt::ControlModifier) && key == Qt::Key_G) return true;
    }

    return QWidget::eventFilter(obj, event);
}

void SecureBrowser::setupProfile()
{
    // Use off-the-record profile (no persistent storage)
    m_profile = new QWebEngineProfile(this);
    m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    // Block downloads
    connect(m_profile, &QWebEngineProfile::downloadRequested,
            this, [](QWebEngineDownloadRequest* download) {
        qWarning() << "Download blocked:" << download->url().toString();
        download->cancel();
    });
}

void SecureBrowser::setupToolbar()
{
    m_toolbar = new QWidget(this);
    m_toolbar->setFixedHeight(36);
    m_toolbar->setStyleSheet("background-color: #2b2b2b; border-bottom: 1px solid #444;");

    auto* layout = new QHBoxLayout(m_toolbar);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    auto makeBtn = [&](const QString& text, auto slot) {
        auto* btn = new QPushButton(text, m_toolbar);
        btn->setFixedSize(60, 28);
        btn->setStyleSheet(
            "QPushButton { background: #3c3c3c; color: #ddd; border: 1px solid #555; "
            "border-radius: 3px; font-size: 12px; } "
            "QPushButton:hover { background: #4c4c4c; } "
            "QPushButton:pressed { background: #555; }");
        connect(btn, &QPushButton::clicked, this, slot);
        layout->addWidget(btn);
        return btn;
    };

    makeBtn("Back", [this]() { m_webView->back(); });
    makeBtn("Forward", [this]() { m_webView->forward(); });
    makeBtn("Reload", [this]() { m_webView->reload(); });
    makeBtn("Stop", [this]() { m_webView->stop(); });

    layout->addStretch();

    auto* titleLabel = new QLabel("OpenLock Secure Browser", m_toolbar);
    titleLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    layout->addWidget(titleLabel);
}

void SecureBrowser::applyHardenedSettings()
{
    if (!m_profile) return;

    QWebEngineSettings* settings = m_profile->settings();

    // Disable risky features
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    settings->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    settings->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
#endif
    settings->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, false);

    // Lock down local content
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);

    // Enable features needed for LMS functionality
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);

    qInfo() << "Browser hardened settings applied";
}

void SecureBrowser::injectConsoleBlocker()
{
    if (!m_page) return;

    QWebEngineScript script;
    script.setName("openlock-console-blocker");
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(QStringLiteral(R"(
        (function() {
            var noop = function(){};
            window.console = {
                log: noop, warn: noop, error: noop, info: noop,
                debug: noop, trace: noop, dir: noop, table: noop,
                time: noop, timeEnd: noop, assert: noop, clear: noop,
                group: noop, groupEnd: noop, groupCollapsed: noop
            };
            Object.freeze(window.console);
        })();
    )"));

    m_page->scripts().insert(script);
}

void SecureBrowser::injectHeaders()
{
    // Header injection is done via SEBRequestInterceptor
    // installed on the QWebEngineProfile in LockdownEngine
}

} // namespace openlock
