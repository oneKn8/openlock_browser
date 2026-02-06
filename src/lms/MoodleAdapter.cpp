// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "lms/LMSAdapter.h"

namespace openlock {

class MoodleAdapter : public LMSAdapter {
    Q_OBJECT

public:
    explicit MoodleAdapter(QObject* parent = nullptr) : LMSAdapter(parent) {}

    LMSType type() const override { return LMSType::Moodle; }
    QString name() const override { return QStringLiteral("Moodle"); }

    bool detectLMS(const QUrl& url) const override
    {
        QString path = url.path().toLower();
        QString host = url.host().toLower();

        return path.contains("/mod/quiz/") ||
               path.contains("/moodle/") ||
               host.contains("moodle");
    }

    QStringList ssoAllowedDomains() const override
    {
        return {};  // Moodle SSO handled by general SSO filter
    }

    QStringList requiredUrlPatterns() const override
    {
        return {
            "*/mod/quiz/*",
            "*/login/*",
            "*/auth/*"
        };
    }

    bool supportsNativeSEB() const override { return true; }
    bool requiresCustomHandshake() const override { return false; }
};

} // namespace openlock

#include "MoodleAdapter.moc"
