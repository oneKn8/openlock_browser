// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QByteArray>
#include <QStringList>

namespace openlock {

class SelfVerifier {
public:
    SelfVerifier();
    ~SelfVerifier();

    bool verifyIntegrity() const;
    QByteArray computeBinaryHash() const;
    QStringList detectInjectedLibraries() const;

    void setExpectedHash(const QByteArray& hash);

private:
    QByteArray m_expectedHash;
};

} // namespace openlock
