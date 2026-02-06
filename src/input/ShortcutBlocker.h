// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>

namespace openlock {

class ShortcutBlocker : public QObject {
    Q_OBJECT

public:
    explicit ShortcutBlocker(QObject* parent = nullptr);
    ~ShortcutBlocker() override = default;

    bool engage();
    bool release();

signals:
    void blocked(const QString& shortcut);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    bool m_active = false;
};

} // namespace openlock
