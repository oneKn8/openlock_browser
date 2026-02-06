// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include "integrity/VMDetector.h"

using namespace openlock;

class VMDetectorTest : public ::testing::Test {
protected:
    VMDetector detector;
};

TEST_F(VMDetectorTest, DetectionReturnsResult) {
    auto result = detector.detect();
    // We can't predict whether the test runs in a VM or not,
    // but we can verify the structure
    EXPECT_GE(result.confidenceScore, 0);
    EXPECT_LE(result.confidenceScore, 100);

    if (result.detected) {
        EXPECT_FALSE(result.hypervisorName.isEmpty());
        EXPECT_GT(result.confidenceScore, 0);
    } else {
        EXPECT_EQ(result.confidenceScore, 0);
    }
}

TEST_F(VMDetectorTest, ConfidenceScoreInRange) {
    auto result = detector.detect();
    EXPECT_GE(result.confidenceScore, 0);
    EXPECT_LE(result.confidenceScore, 100);
}
