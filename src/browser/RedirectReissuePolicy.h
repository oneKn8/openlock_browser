// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QUrl>

namespace openlock {

// Decides which main-frame redirects must be re-issued as fresh navigations in SEB mode.
//
// QtWebEngine (seen on 6.2) sends a redirected request before the request interceptor
// can change or block it: the redirect target gets the X-SafeExamBrowser-*Hash headers
// of the previous URL, so the LMS rejects it, and blocking it in the interceptor is too
// late because the server has already run it (Moodle's access log shows the leg, then a
// client abort). On Moodle's login a duplicated "testsession" leg uses up the page the
// student asked for and lands them on the Dashboard instead of the quiz.
//
// So redirects are refused in acceptNavigationRequest(), before Chromium follows them,
// and the target is loaded again as a new navigation, which the interceptor hashes for
// the URL actually requested. A new navigation is a GET: a 307/308 after a POST loses its
// method and body (LMS form posts answer with 302/303, which browsers follow with GET
// anyway). Kept free of QtWebEngine types for unit tests.
class RedirectReissuePolicy {
public:
    // Matches Chromium's own redirect limit.
    static constexpr int kMaxReissues = 20;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // From acceptNavigationRequest() for every main-frame navigation that passed the
    // other checks. True: refuse this navigation and load `url` as a new navigation.
    bool shouldReissue(const QUrl& url, bool isRedirect)
    {
        if (!isRedirect) {
            if (!m_reissuedUrl.isEmpty() && sameRequest(url, m_reissuedUrl)) {
                // Our own re-issued load: same redirect chain, keep counting.
                m_reissuedUrl.clear();
                return false;
            }
            m_reissuedUrl.clear();
            m_reissues = 0;
            return false;
        }
        if (!m_enabled || m_reissues >= kMaxReissues) {
            // Past the cap, Chromium's own redirect limit ends the loop.
            return false;
        }
        ++m_reissues;
        m_reissuedUrl = url;
        return true;
    }

private:
    // The navigation can carry a fragment the network request never does.
    static bool sameRequest(const QUrl& a, const QUrl& b)
    {
        return a.adjusted(QUrl::RemoveFragment) == b.adjusted(QUrl::RemoveFragment);
    }

    bool m_enabled = false;
    QUrl m_reissuedUrl;
    int m_reissues = 0;
};

} // namespace openlock
