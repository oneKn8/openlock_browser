// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>

class QTimer;

namespace openlock {

class ClipboardGuard : public QObject {
    Q_OBJECT

public:
    explicit ClipboardGuard(QObject* parent = nullptr);
    ~ClipboardGuard() override = default;

    bool engage();
    bool release();

signals:
    void violation();

private:
    void clearClipboard();

    QTimer* m_timer = nullptr;
    bool m_active = false;
};

} // namespace openlock
