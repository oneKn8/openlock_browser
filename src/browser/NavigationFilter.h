// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QRegularExpression>
#include <QList>

namespace openlock {

enum class FilterResult {
    Allowed,
    Blocked,
    AllowedSSO    // Allowed as SSO redirect
};

class NavigationFilter : public QObject {
    Q_OBJECT

public:
    explicit NavigationFilter(QObject* parent = nullptr);
    ~NavigationFilter() override;

    FilterResult checkUrl(const QUrl& url) const;

    void addAllowedPattern(const QString& pattern);
    void addBlockedPattern(const QString& pattern);
    void addSSODomain(const QString& domain);

    void setAllowedPatterns(const QStringList& patterns);
    void setBlockedPatterns(const QStringList& patterns);
    void setSSODomains(const QStringList& domains);

signals:
    void urlBlocked(const QUrl& url, const QString& reason);
    void urlAllowed(const QUrl& url);

private:
    bool matchesPattern(const QUrl& url, const QList<QRegularExpression>& patterns) const;
    static QString globToUrlRegex(const QString& glob);
    bool isSSODomain(const QUrl& url) const;
    bool isBlockedScheme(const QUrl& url) const;

    QList<QRegularExpression> m_allowedPatterns;
    QList<QRegularExpression> m_blockedPatterns;
    QStringList m_ssoDomains;
};

} // namespace openlock
