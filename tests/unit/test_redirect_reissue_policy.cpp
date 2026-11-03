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
    EXPECT_FALSE(p.shouldReissue(kStart, false));
    return p;
}
} // namespace

TEST(RedirectReissuePolicyTest, RefusesRedirectsAndAcceptsTheReissuedLoad)
{
    auto p = enabledPolicy();
    EXPECT_TRUE(p.shouldReissue(kTarget, true));
    // The re-issued load is a normal navigation and goes through.
    EXPECT_FALSE(p.shouldReissue(kTarget, false));
}

// Moodle's login: POST -> 303 testsession -> 303 wanted page. Each leg is refused before
// it is sent, so the server sees every URL once.
TEST(RedirectReissuePolicyTest, RefusesEveryLegOfAChain)
{
    auto p = enabledPolicy();
    const QUrl test(QStringLiteral("http://lms.example/login/index.php?testsession=3"));
    EXPECT_TRUE(p.shouldReissue(test, true));
    EXPECT_FALSE(p.shouldReissue(test, false));
    EXPECT_TRUE(p.shouldReissue(kTarget, true));
    EXPECT_FALSE(p.shouldReissue(kTarget, false));
}

TEST(RedirectReissuePolicyTest, LeavesNavigationsThatAreNotRedirectsAlone)
{
    auto p = enabledPolicy();
    EXPECT_FALSE(p.shouldReissue(kTarget, false));
    EXPECT_FALSE(p.shouldReissue(kStart, false));
}

TEST(RedirectReissuePolicyTest, DoesNothingWhenDisabled)
{
    RedirectReissuePolicy p;
    EXPECT_FALSE(p.shouldReissue(kStart, false));
    EXPECT_FALSE(p.shouldReissue(kTarget, true));
}

TEST(RedirectReissuePolicyTest, CapsALoopAndResetsOnANewNavigation)
{
    auto p = enabledPolicy();
    int reissued = 0;
    for (int i = 0; i < RedirectReissuePolicy::kMaxReissues + 5; ++i) {
        const QUrl hop(QStringLiteral("http://lms.example/loop/%1").arg(i));
        if (p.shouldReissue(hop, true)) {
            ++reissued;
            EXPECT_FALSE(p.shouldReissue(hop, false));  // our own reload
        }
    }
    EXPECT_EQ(reissued, RedirectReissuePolicy::kMaxReissues);

    // A navigation the user or page starts begins a new chain.
    EXPECT_FALSE(p.shouldReissue(kStart, false));
    EXPECT_TRUE(p.shouldReissue(kTarget, true));
}

// The redirect target can carry a fragment; the reload with it is still our own load.
TEST(RedirectReissuePolicyTest, MatchesReissuedLoadsWithFragments)
{
    auto p = enabledPolicy();
    const QUrl withFragment(QStringLiteral("http://lms.example/mod/quiz/view.php?id=1#region-main"));
    for (int i = 0; i < RedirectReissuePolicy::kMaxReissues - 1; ++i) {
        ASSERT_TRUE(p.shouldReissue(withFragment, true));
        EXPECT_FALSE(p.shouldReissue(kTarget, false));
    }
    // Still the same chain: one re-issue left before the cap.
    EXPECT_TRUE(p.shouldReissue(withFragment, true));
    EXPECT_FALSE(p.shouldReissue(withFragment, false));
    EXPECT_FALSE(p.shouldReissue(withFragment, true)) << "cap reached";
}
