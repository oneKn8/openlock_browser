// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "lms/LMSAdapter.h"

namespace openlock {

class CanvasAdapter : public LMSAdapter {
    Q_OBJECT

public:
    explicit CanvasAdapter(QObject* parent = nullptr) : LMSAdapter(parent) {}

    LMSType type() const override { return LMSType::Canvas; }
    QString name() const override { return QStringLiteral("Canvas"); }

    bool detectLMS(const QUrl& url) const override
    {
        QString host = url.host().toLower();
        QString path = url.path().toLower();

        return host.contains("instructure.com") ||
               host.contains("canvas") ||
               path.contains("/courses/") ||
               path.contains("/quizzes/");
    }

    QStringList ssoAllowedDomains() const override
    {
        return { "*.instructure.com" };
    }

    QStringList requiredUrlPatterns() const override
    {
        return {
            "*/courses/*/quizzes/*",
            "*/courses/*/assignments/*",
            "*/login/*"
        };
    }

    bool supportsNativeSEB() const override { return false; }
    bool requiresCustomHandshake() const override { return true; }
};

} // namespace openlock

#include "CanvasAdapter.moc"
