// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include "browser/NavigationFilter.h"

#include <QCoreApplication>
#include <QUrl>

using namespace openlock;

class NavigationFilterTest : public ::testing::Test {
protected:
    NavigationFilter filter;

    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = { const_cast<char*>("test") };
            static QCoreApplication app(argc, argv);
        }
    }
};

TEST_F(NavigationFilterTest, BlocksDangerousSchemes) {
    EXPECT_EQ(filter.checkUrl(QUrl("file:///etc/passwd")), FilterResult::Blocked);
    EXPECT_EQ(filter.checkUrl(QUrl("about:blank")), FilterResult::Blocked);
    EXPECT_EQ(filter.checkUrl(QUrl("chrome://settings")), FilterResult::Blocked);
    EXPECT_EQ(filter.checkUrl(QUrl("javascript:alert(1)")), FilterResult::Blocked);
    EXPECT_EQ(filter.checkUrl(QUrl("data:text/html,<h1>hi</h1>")), FilterResult::Blocked);
    EXPECT_EQ(filter.checkUrl(QUrl("view-source:https://example.com")), FilterResult::Blocked);
}

TEST_F(NavigationFilterTest, AllowsHttpsByDefault) {
    // No whitelist configured = allow all non-blocked
    EXPECT_EQ(filter.checkUrl(QUrl("https://example.com")), FilterResult::Allowed);
    EXPECT_EQ(filter.checkUrl(QUrl("https://moodle.school.edu/quiz")), FilterResult::Allowed);
}

TEST_F(NavigationFilterTest, WhitelistMode) {
    filter.addAllowedPattern("*.example.com/*");

    EXPECT_EQ(filter.checkUrl(QUrl("https://www.example.com/quiz")), FilterResult::Allowed);
    EXPECT_EQ(filter.checkUrl(QUrl("https://other.com/page")), FilterResult::Blocked);
}

TEST_F(NavigationFilterTest, SSODomainsAlwaysAllowed) {
    filter.addAllowedPattern("*.moodle.edu/*");

    // SSO domains should pass even if not in whitelist
    EXPECT_EQ(filter.checkUrl(QUrl("https://login.microsoftonline.com/auth")), FilterResult::AllowedSSO);
    EXPECT_EQ(filter.checkUrl(QUrl("https://accounts.google.com/signin")), FilterResult::AllowedSSO);
}

TEST_F(NavigationFilterTest, CustomSSODomain) {
    filter.addAllowedPattern("*.school.edu/*");
    filter.addSSODomain("idp.school.edu");

    EXPECT_EQ(filter.checkUrl(QUrl("https://idp.school.edu/shibboleth")), FilterResult::AllowedSSO);
}

TEST_F(NavigationFilterTest, BlocklistOverridesAllowlist) {
    filter.addAllowedPattern("*.example.com/*");
    filter.addBlockedPattern("*.example.com/admin/*");

    EXPECT_EQ(filter.checkUrl(QUrl("https://www.example.com/quiz")), FilterResult::Allowed);
    EXPECT_EQ(filter.checkUrl(QUrl("https://www.example.com/admin/panel")), FilterResult::Blocked);
}
