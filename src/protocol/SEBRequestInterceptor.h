// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QWebEngineUrlRequestInterceptor>
#include <QByteArray>

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

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

private:
    bool isBlockedScheme(const QString& scheme) const;

    SEBProtocol* m_protocol = nullptr;
    NavigationFilter* m_navFilter = nullptr;
};

} // namespace openlock
