// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "protocol/SEBRequestInterceptor.h"
#include "protocol/SEBProtocol.h"
#include "browser/NavigationFilter.h"

#include <QDebug>

namespace openlock {

SEBRequestInterceptor::SEBRequestInterceptor(QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

SEBRequestInterceptor::~SEBRequestInterceptor() = default;

void SEBRequestInterceptor::setSEBProtocol(SEBProtocol* protocol)
{
    m_protocol = protocol;
}

void SEBRequestInterceptor::setNavigationFilter(NavigationFilter* filter)
{
    m_navFilter = filter;
}

void SEBRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    QUrl url = info.requestUrl();
    QString scheme = url.scheme().toLower();

    // Block dangerous URL schemes
    if (isBlockedScheme(scheme)) {
        info.block(true);
        return;
    }

    // Only allow http/https
    if (scheme != "http" && scheme != "https") {
        info.block(true);
        return;
    }

    // Check navigation filter for sub-resource requests
    if (m_navFilter) {
        auto result = m_navFilter->checkUrl(url);
        if (result == FilterResult::Blocked) {
            info.block(true);
            return;
        }
    }

    // Inject SEB headers if protocol is active
    if (m_protocol) {
        // Browser Exam Key request hash — URL-specific, hex-encoded
        QByteArray requestHash = m_protocol->computeRequestHash(url);
        if (!requestHash.isEmpty()) {
            info.setHttpHeader(
                SEBProtocol::requestHashHeaderName().toUtf8(),
                requestHash
            );
        }

        // Config Key request hash — URL-specific, hex-encoded
        QByteArray configKeyHash = m_protocol->computeConfigKeyHash(url);
        if (!configKeyHash.isEmpty()) {
            info.setHttpHeader(
                SEBProtocol::configKeyHeaderName().toUtf8(),
                configKeyHash
            );
        }
    }
}

bool SEBRequestInterceptor::isBlockedScheme(const QString& scheme) const
{
    static const QStringList blocked = {
        "file", "about", "chrome", "chrome-devtools",
        "data", "blob", "javascript", "ftp", "view-source"
    };
    return blocked.contains(scheme);
}

} // namespace openlock
