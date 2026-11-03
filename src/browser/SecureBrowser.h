// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <memory>

namespace openlock {

class NavigationFilter;
class DownloadBlocker;
class DevToolsBlocker;
class Config;

class SecurePage : public QWebEnginePage {
    Q_OBJECT

public:
    explicit SecurePage(QWebEngineProfile* profile, QObject* parent = nullptr);

    // When SEB headers are injected, main-frame redirects are re-issued as fresh loads so
    // the per-URL key hashes are computed for the redirect target (see the .cpp).
    void setReissueRedirects(bool enabled);

    // Upper bound on consecutive re-issued redirects in one navigation chain; matches
    // Chromium's own redirect limit.
    static constexpr int kMaxReissuedRedirects = 20;

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;
    QWebEnginePage* createWindow(WebWindowType type) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString& message, int lineNumber,
                                  const QString& sourceID) override;
    void triggerAction(WebAction action, bool checked = false) override;

private:
    bool m_reissueRedirects = false;
    int m_reissuedRedirects = 0;
    QUrl m_reissuedUrl;
};

class SecureBrowser : public QWidget {
    Q_OBJECT

public:
    explicit SecureBrowser(QWidget* parent = nullptr);
    ~SecureBrowser() override;

    bool initialize(const Config* config);
    void navigateTo(const QUrl& url);
    QUrl currentUrl() const;

    void setNavigationFilter(NavigationFilter* filter);
    NavigationFilter* navigationFilter() const;

    QWebEngineView* webView() const;

signals:
    void urlChanged(const QUrl& url);
    void navigationBlocked(const QUrl& url, const QString& reason);
    void pageLoadStarted();
    void pageLoadFinished(bool ok);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupProfile();
    void setupToolbar();
    void applyHardenedSettings();
    void injectHeaders();
    void injectConsoleBlocker();

    QWebEngineView* m_webView = nullptr;
    QWebEngineProfile* m_profile = nullptr;
    SecurePage* m_page = nullptr;
    NavigationFilter* m_navFilter = nullptr;
    std::unique_ptr<DownloadBlocker> m_downloadBlocker;
    std::unique_ptr<DevToolsBlocker> m_devToolsBlocker;

    QWidget* m_toolbar = nullptr;
};

} // namespace openlock
