// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>

namespace openlock {

class PrintBlocker : public QObject {
    Q_OBJECT

public:
    explicit PrintBlocker(QObject* parent = nullptr);
    ~PrintBlocker() override = default;

    bool engage();
    bool release();

private:
    bool m_active = false;
    bool m_cupsWasStopped = false;
};

} // namespace openlock
