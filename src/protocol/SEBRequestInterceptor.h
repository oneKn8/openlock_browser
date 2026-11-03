// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QWebEngineUrlRequestInterceptor>
#include <QByteArray>
#include <QUrl>

#include <functional>

namespace openlock {

class SEBProtocol;
class NavigationFilter;

class SEBRequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    explicit SEBRequestInterceptor(QObject* parent = nullptr);
    ~SEBRequestInterceptor() override;

    void setSEBProtocol(SEBProtocol* protocol);
    void setNavigationFilter(NavigationFilter* filter);

    // Asked for every main-frame request; returning true blocks it (the hook's owner
    // re-issues it). Used to resend redirect targets with fresh SEB hashes.
    using MainFrameRequestHook = std::function<bool(const QUrl& url, const QByteArray& method)>;
    void setMainFrameRequestHook(MainFrameRequestHook hook);

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

private:
    bool isBlockedScheme(const QString& scheme) const;

    SEBProtocol* m_protocol = nullptr;
    NavigationFilter* m_navFilter = nullptr;
    MainFrameRequestHook m_mainFrameHook;
};

} // namespace openlock
