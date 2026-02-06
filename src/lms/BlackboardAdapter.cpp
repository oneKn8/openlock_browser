// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "lms/LMSAdapter.h"

namespace openlock {

class BlackboardAdapter : public LMSAdapter {
    Q_OBJECT

public:
    explicit BlackboardAdapter(QObject* parent = nullptr) : LMSAdapter(parent) {}

    LMSType type() const override { return LMSType::Blackboard; }
    QString name() const override { return QStringLiteral("Blackboard"); }

    bool detectLMS(const QUrl& url) const override
    {
        QString host = url.host().toLower();
        return host.contains("blackboard") || host.contains("bblearn");
    }

    QStringList ssoAllowedDomains() const override
    {
        return { "*.blackboard.com" };
    }

    QStringList requiredUrlPatterns() const override
    {
        return {
            "*/webapps/assessment/*",
            "*/webapps/blackboard/*",
            "*/ultra/*"
        };
    }

    bool supportsNativeSEB() const override { return false; }
    bool requiresCustomHandshake() const override { return true; }
};

LMSType LMSAdapter::detectFromUrl(const QUrl& url)
{
    QString host = url.host().toLower();
    QString path = url.path().toLower();

    if (host.contains("moodle") || path.contains("/moodle/"))
        return LMSType::Moodle;
    if (host.contains("instructure.com") || host.contains("canvas"))
        return LMSType::Canvas;
    if (host.contains("blackboard") || host.contains("bblearn"))
        return LMSType::Blackboard;
    if (host.contains("brightspace") || host.contains("d2l"))
        return LMSType::Brightspace;
    if (host.contains("sakai"))
        return LMSType::Sakai;
    if (host.contains("schoology"))
        return LMSType::Schoology;

    return LMSType::Unknown;
}

} // namespace openlock

#include "BlackboardAdapter.moc"
