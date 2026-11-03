// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "browser/RedirectReissuePolicy.h"

using openlock::RedirectReissuePolicy;

namespace {
const QUrl kStart(QStringLiteral("http://lms.example/start"));
const QUrl kTarget(QStringLiteral("http://lms.example/mod/quiz/view.php?id=1"));

RedirectReissuePolicy enabledPolicy()
{
    RedirectReissuePolicy p;
    p.setEnabled(true);
    p.onMainFrameNavigation(kStart, false);
    return p;
}
} // namespace

TEST(RedirectReissuePolicyTest, ReissuesGetLegOfRedirect)
{
    auto p = enabledPolicy();
    p.onMainFrameNavigation(kTarget, true);
    EXPECT_TRUE(p.shouldReissue(kTarget, "GET"));
    // The re-issued load is a normal navigation and must not be re-issued again.
    p.onMainFrameNavigation(kTarget, false);
    EXPECT_FALSE(p.shouldReissue(kTarget, "GET"));
}

// 307/308 after a POST: Chromium repeats the POST with its body; re-issuing would turn it
// into a GET and drop the body.
TEST(RedirectReissuePolicyTest, LeavesMethodPreservingRedirectsAlone)
{
    auto p = enabledPolicy();
    p.onMainFrameNavigation(kTarget, true);
    EXPECT_FALSE(p.shouldReissue(kTarget, "POST"));
    EXPECT_FALSE(p.shouldReissue(kTarget, "GET")) << "a pending redirect is consumed once";
}

TEST(RedirectReissuePolicyTest, IgnoresRequestsThatAreNotTheRedirectTarget)
{
    auto p = enabledPolicy();
    EXPECT_FALSE(p.shouldReissue(kTarget, "GET")) << "no redirect was reported";
    p.onMainFrameNavigation(kTarget, true);
    EXPECT_FALSE(p.shouldReissue(QUrl(QStringLiteral("http://lms.example/other")), "GET"));
}

TEST(RedirectReissuePolicyTest, DoesNothingWhenDisabled)
{
    RedirectReissuePolicy p;
    p.onMainFrameNavigation(kStart, false);
    p.onMainFrameNavigation(kTarget, true);
    EXPECT_FALSE(p.shouldReissue(kTarget, "GET"));
}

TEST(RedirectReissuePolicyTest, CapsALoopAndResetsOnANewNavigation)
{
    auto p = enabledPolicy();
    int reissued = 0;
    for (int i = 0; i < RedirectReissuePolicy::kMaxReissues + 5; ++i) {
        const QUrl hop(QStringLiteral("http://lms.example/loop/%1").arg(i));
        p.onMainFrameNavigation(hop, true);
        if (p.shouldReissue(hop, "GET")) {
            ++reissued;
            p.onMainFrameNavigation(hop, false);  // our own reload
        }
    }
    EXPECT_EQ(reissued, RedirectReissuePolicy::kMaxReissues);

    // A navigation the user or page starts begins a new chain.
    p.onMainFrameNavigation(kStart, false);
    p.onMainFrameNavigation(kTarget, true);
    EXPECT_TRUE(p.shouldReissue(kTarget, "GET"));
}
