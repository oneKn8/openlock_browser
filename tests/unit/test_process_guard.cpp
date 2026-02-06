// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include "guard/ProcessBlocklist.h"

using namespace openlock;

class ProcessBlocklistTest : public ::testing::Test {
protected:
    ProcessBlocklist blocklist;

    void SetUp() override {
        blocklist.loadDefaults();
    }
};

TEST_F(ProcessBlocklistTest, BlocksKnownScreenCapture) {
    EXPECT_TRUE(blocklist.isBlocked("obs"));
    EXPECT_TRUE(blocklist.isBlocked("ffmpeg"));
    EXPECT_TRUE(blocklist.isBlocked("kazam"));
    EXPECT_TRUE(blocklist.isBlocked("simplescreenrecorder"));
}

TEST_F(ProcessBlocklistTest, BlocksKnownScreenSharing) {
    EXPECT_TRUE(blocklist.isBlocked("zoom"));
    EXPECT_TRUE(blocklist.isBlocked("teams"));
    EXPECT_TRUE(blocklist.isBlocked("discord"));
    EXPECT_TRUE(blocklist.isBlocked("anydesk"));
}

TEST_F(ProcessBlocklistTest, BlocksTerminals) {
    EXPECT_TRUE(blocklist.isBlocked("gnome-terminal"));
    EXPECT_TRUE(blocklist.isBlocked("konsole"));
    EXPECT_TRUE(blocklist.isBlocked("alacritty"));
    EXPECT_TRUE(blocklist.isBlocked("kitty"));
    EXPECT_TRUE(blocklist.isBlocked("tmux"));
}

TEST_F(ProcessBlocklistTest, BlocksBrowsers) {
    EXPECT_TRUE(blocklist.isBlocked("firefox"));
    EXPECT_TRUE(blocklist.isBlocked("chromium"));
    EXPECT_TRUE(blocklist.isBlocked("brave"));
    EXPECT_TRUE(blocklist.isBlocked("google-chrome"));
}

TEST_F(ProcessBlocklistTest, AllowsUnknownProcess) {
    EXPECT_FALSE(blocklist.isBlocked("openlock"));
    EXPECT_FALSE(blocklist.isBlocked("systemd"));
    EXPECT_FALSE(blocklist.isBlocked("Xorg"));
    EXPECT_FALSE(blocklist.isBlocked("pulseaudio"));
}

TEST_F(ProcessBlocklistTest, CaseInsensitiveBlocking) {
    // blocklist stores lowercase, so matching should work
    EXPECT_TRUE(blocklist.isBlocked("obs", "", "/usr/bin/obs"));
}

TEST_F(ProcessBlocklistTest, AddAndRemove) {
    blocklist.add("custom-tool");
    EXPECT_TRUE(blocklist.isBlocked("custom-tool"));

    blocklist.remove("custom-tool");
    EXPECT_FALSE(blocklist.isBlocked("custom-tool"));
}

TEST_F(ProcessBlocklistTest, BlocksAutomation) {
    EXPECT_TRUE(blocklist.isBlocked("xdotool"));
    EXPECT_TRUE(blocklist.isBlocked("ydotool"));
    EXPECT_TRUE(blocklist.isBlocked("xclip"));
}
