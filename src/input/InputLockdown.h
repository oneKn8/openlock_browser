// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <memory>

namespace openlock {

class ClipboardGuard;
class ShortcutBlocker;
class PrintBlocker;

class InputLockdown : public QObject {
    Q_OBJECT

public:
    explicit InputLockdown(QObject* parent = nullptr);
    ~InputLockdown() override;

    bool engage();
    bool release();
    bool isEngaged() const;

    void setClipboardAllowed(bool allowed);
    void setPrintAllowed(bool allowed);

    ClipboardGuard* clipboardGuard() const;
    ShortcutBlocker* shortcutBlocker() const;

signals:
    void engaged();
    void released();
    void shortcutBlocked(const QString& shortcutName);
    void clipboardViolation();

private:
    bool grabKeyboard();
    bool ungrabKeyboard();

    std::unique_ptr<ClipboardGuard> m_clipboardGuard;
    std::unique_ptr<ShortcutBlocker> m_shortcutBlocker;
    std::unique_ptr<PrintBlocker> m_printBlocker;
    bool m_engaged = false;
};

} // namespace openlock
