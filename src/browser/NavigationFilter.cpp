// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "browser/NavigationFilter.h"

#include <QDebug>

namespace openlock {

NavigationFilter::NavigationFilter(QObject* parent)
    : QObject(parent)
{
    // Default SSO domains that should always be allowed for auth redirects
    m_ssoDomains = {
        "login.microsoftonline.com",
        "accounts.google.com",
        "auth.google.com",
        "shibboleth",
        "idp.",
        "cas.",
        "login.",
        "auth.",
        "sso.",
        "adfs.",
        "okta.com",
        "onelogin.com",
        "ping.",
    };
}

NavigationFilter::~NavigationFilter() = default;

FilterResult NavigationFilter::checkUrl(const QUrl& url) const
{
    // Block dangerous schemes
    if (isBlockedScheme(url)) {
        return FilterResult::Blocked;
    }

    // Always allow SSO domains for authentication
    if (isSSODomain(url)) {
        return FilterResult::AllowedSSO;
    }

    // Check blocked patterns first (explicit blocks override allows)
    if (matchesPattern(url, m_blockedPatterns)) {
        return FilterResult::Blocked;
    }

    // If we have allowed patterns, URL must match at least one
    if (!m_allowedPatterns.isEmpty()) {
        if (matchesPattern(url, m_allowedPatterns)) {
            return FilterResult::Allowed;
        }
        return FilterResult::Blocked;
    }

    // No whitelist configured — allow everything not explicitly blocked
    return FilterResult::Allowed;
}

void NavigationFilter::addAllowedPattern(const QString& pattern)
{
    m_allowedPatterns.append(QRegularExpression(
        globToUrlRegex(pattern),
        QRegularExpression::CaseInsensitiveOption));
}

void NavigationFilter::addBlockedPattern(const QString& pattern)
{
    m_blockedPatterns.append(QRegularExpression(
        globToUrlRegex(pattern),
        QRegularExpression::CaseInsensitiveOption));
}

void NavigationFilter::addSSODomain(const QString& domain)
{
    m_ssoDomains.append(domain);
}

void NavigationFilter::setAllowedPatterns(const QStringList& patterns)
{
    m_allowedPatterns.clear();
    for (const auto& p : patterns) {
        addAllowedPattern(p);
    }
}

void NavigationFilter::setBlockedPatterns(const QStringList& patterns)
{
    m_blockedPatterns.clear();
    for (const auto& p : patterns) {
        addBlockedPattern(p);
    }
}

void NavigationFilter::setSSODomains(const QStringList& domains)
{
    m_ssoDomains = domains;
}

QString NavigationFilter::globToUrlRegex(const QString& glob)
{
    // URL-aware glob: * matches any characters (including /)
    QString regex;
    regex.reserve(glob.size() * 2);
    for (QChar c : glob) {
        if (c == '*') regex += ".*";
        else if (c == '?') regex += '.';
        else regex += QRegularExpression::escape(QString(c));
    }
    return regex;
}

bool NavigationFilter::matchesPattern(const QUrl& url, const QList<QRegularExpression>& patterns) const
{
    QString urlStr = url.toString();
    for (const auto& pattern : patterns) {
        if (pattern.match(urlStr).hasMatch()) {
            return true;
        }
    }
    return false;
}

bool NavigationFilter::isSSODomain(const QUrl& url) const
{
    QString host = url.host().toLower();
    for (const QString& ssoDomain : m_ssoDomains) {
        if (host.contains(ssoDomain, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool NavigationFilter::isBlockedScheme(const QUrl& url) const
{
    QString scheme = url.scheme().toLower();
    return scheme == "file" || scheme == "about" || scheme == "chrome" ||
           scheme == "data" || scheme == "javascript" || scheme == "view-source" ||
           scheme == "ftp";
}

} // namespace openlock
