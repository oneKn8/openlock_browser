// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QString>
#include <QSet>
#include <QList>
#include <QRegularExpression>

namespace openlock {

class ProcessBlocklist {
public:
    ProcessBlocklist();
    ~ProcessBlocklist();

    bool loadFromFile(const QString& path);
    void loadDefaults();

    void add(const QString& name);
    void remove(const QString& name);
    bool isBlocked(const QString& name, const QString& cmdline = {}, const QString& exe = {}) const;

    int size() const { return m_blockedNames.size(); }

private:
    QSet<QString> m_blockedNames;
    QList<QRegularExpression> m_patterns;
};

} // namespace openlock
