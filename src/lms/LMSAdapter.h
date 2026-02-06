// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QUrl>
#include <QString>

namespace openlock {

enum class LMSType {
    Unknown,
    Moodle,
    Canvas,
    Blackboard,
    Brightspace,
    Sakai,
    Schoology
};

class LMSAdapter : public QObject {
    Q_OBJECT

public:
    explicit LMSAdapter(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~LMSAdapter() = default;

    virtual LMSType type() const = 0;
    virtual QString name() const = 0;

    virtual bool detectLMS(const QUrl& url) const = 0;
    virtual QStringList ssoAllowedDomains() const = 0;
    virtual QStringList requiredUrlPatterns() const = 0;

    virtual bool supportsNativeSEB() const = 0;
    virtual bool requiresCustomHandshake() const = 0;

    static LMSType detectFromUrl(const QUrl& url);
};

} // namespace openlock
