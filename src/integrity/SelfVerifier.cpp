// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "integrity/SelfVerifier.h"

#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>

#include <unistd.h>

namespace openlock {

SelfVerifier::SelfVerifier() = default;
SelfVerifier::~SelfVerifier() = default;

bool SelfVerifier::verifyIntegrity() const
{
    if (m_expectedHash.isEmpty()) {
        // No expected hash set — skip verification
        return true;
    }

    QByteArray currentHash = computeBinaryHash();
    if (currentHash.isEmpty()) {
        qWarning() << "Failed to compute binary hash";
        return false;
    }

    return currentHash == m_expectedHash;
}

QByteArray SelfVerifier::computeBinaryHash() const
{
    // Get our own executable path
    QString exePath = QCoreApplication::applicationFilePath();
    if (exePath.isEmpty()) {
        // Fallback: read /proc/self/exe
        exePath = QFile::symLinkTarget("/proc/self/exe");
    }

    QFile file(exePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open own binary for hashing:" << exePath;
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        qWarning() << "Failed to hash binary";
        return {};
    }

    return hash.result();
}

QStringList SelfVerifier::detectInjectedLibraries() const
{
    QStringList suspicious;

    // Known legitimate library paths
    QStringList legitimatePaths = {
        "/usr/lib", "/usr/lib64", "/lib", "/lib64",
        "/usr/local/lib", "/usr/share",
        "/snap/", "/opt/qt"
    };

    QFile mapsFile("/proc/self/maps");
    if (!mapsFile.open(QIODevice::ReadOnly)) return suspicious;

    QTextStream stream(&mapsFile);
    QString line;
    while (stream.readLineInto(&line)) {
        // Look for mapped shared libraries
        if (!line.contains(".so")) continue;

        // Extract the path (last field in maps entry)
        int pathStart = line.indexOf('/');
        if (pathStart < 0) continue;

        QString libPath = line.mid(pathStart).trimmed();

        // Check if it's from a known legitimate location
        bool legitimate = false;
        for (const QString& prefix : legitimatePaths) {
            if (libPath.startsWith(prefix)) {
                legitimate = true;
                break;
            }
        }

        if (!legitimate && !libPath.isEmpty()) {
            if (!suspicious.contains(libPath)) {
                suspicious.append(libPath);
                qWarning() << "Suspicious library mapped:" << libPath;
            }
        }
    }

    return suspicious;
}

void SelfVerifier::setExpectedHash(const QByteArray& hash)
{
    m_expectedHash = hash;
}

} // namespace openlock
