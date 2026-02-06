// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QString>

namespace openlock {

struct VMDetectionResult {
    bool detected = false;
    QString hypervisorName;
    int confidenceScore = 0;  // 0-100, higher = more confident
};

class VMDetector {
public:
    VMDetector();
    ~VMDetector();

    VMDetectionResult detect() const;

private:
    bool checkSystemdDetectVirt(VMDetectionResult& result) const;
    bool checkCPUID(VMDetectionResult& result) const;
    bool checkDMI(VMDetectionResult& result) const;
    bool checkScsiDevices(VMDetectionResult& result) const;
    bool checkMACAddress(VMDetectionResult& result) const;
    bool checkKernelModules(VMDetectionResult& result) const;
    bool checkProcCpuinfo(VMDetectionResult& result) const;
};

} // namespace openlock
