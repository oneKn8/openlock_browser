// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QByteArray>
#include <QUrl>

namespace openlock {

// Decides which main-frame redirects must be re-issued as fresh requests in SEB mode.
//
// QtWebEngine (seen on 6.2) keeps the headers the request interceptor set for the original
// request when it follows a redirect, so the redirect target would be sent the
// X-SafeExamBrowser-*Hash of the previous URL and the LMS would reject it. Re-issuing the
// target as a new navigation makes the interceptor hash the URL actually requested.
//
// Only GET legs are re-issued, because a new navigation is always a GET. A 307/308 after a
// POST keeps its method and body and is left to Chromium (its hash stays stale; LMS logins
// answer a POST with 302/303, which arrive here as GET).
//
// Flow: SecurePage reports every main-frame navigation decision; the request interceptor
// asks shouldReissue() for every main-frame request and blocks it when told to, after
// which the caller loads the URL again. Kept free of QtWebEngine types for unit tests.
class RedirectReissuePolicy {
public:
    // Matches Chromium's own redirect limit.
    static constexpr int kMaxReissues = 20;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // From acceptNavigationRequest() for main-frame navigations that are allowed.
    void onMainFrameNavigation(const QUrl& url, bool isRedirect)
    {
        if (isRedirect) {
            if (m_enabled) {
                m_pendingRedirect = url;
            }
            return;
        }
        m_pendingRedirect.clear();
        if (!m_reissuedUrl.isEmpty() && url == m_reissuedUrl) {
            // Our own re-issued load: same redirect chain, keep counting.
            m_reissuedUrl.clear();
            return;
        }
        m_reissuedUrl.clear();
        m_reissues = 0;
    }

    // From the request interceptor for a main-frame request. True: block this request and
    // load `url` again as a new navigation.
    bool shouldReissue(const QUrl& url, const QByteArray& method)
    {
        if (!m_enabled || m_pendingRedirect.isEmpty() || url != m_pendingRedirect) {
            return false;
        }
        m_pendingRedirect.clear();
        if (method != QByteArrayLiteral("GET")) {
            return false;
        }
        if (m_reissues >= kMaxReissues) {
            // Leave the loop to Chromium, which stops it with its own redirect limit.
            return false;
        }
        ++m_reissues;
        m_reissuedUrl = url;
        return true;
    }

private:
    bool m_enabled = false;
    QUrl m_pendingRedirect;
    QUrl m_reissuedUrl;
    int m_reissues = 0;
};

} // namespace openlock
