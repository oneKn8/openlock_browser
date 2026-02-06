// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "guard/ProcessBlocklist.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>

namespace openlock {

ProcessBlocklist::ProcessBlocklist() = default;
ProcessBlocklist::~ProcessBlocklist() = default;

bool ProcessBlocklist::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open blocklist file:" << path << "- using built-in defaults";
        loadDefaults();
        return true;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Blocklist JSON parse error:" << error.errorString();
        loadDefaults();
        return true;
    }

    QJsonObject root = doc.object();

    auto loadCategory = [&](const QString& key) {
        for (const auto& v : root[key].toArray()) {
            m_blockedNames.insert(v.toString().toLower());
        }
    };

    loadCategory("screen_capture");
    loadCategory("screen_sharing");
    loadCategory("messaging");
    loadCategory("virtual_machines");
    loadCategory("remote_desktop");
    loadCategory("terminals");
    loadCategory("browsers");
    loadCategory("automation");

    // Load regex patterns
    for (const auto& v : root["patterns"].toArray()) {
        m_patterns.append(QRegularExpression(v.toString(), QRegularExpression::CaseInsensitiveOption));
    }

    qInfo() << "Loaded blocklist:" << m_blockedNames.size() << "names," << m_patterns.size() << "patterns";
    return true;
}

void ProcessBlocklist::add(const QString& name)
{
    m_blockedNames.insert(name.toLower());
}

void ProcessBlocklist::remove(const QString& name)
{
    m_blockedNames.remove(name.toLower());
}

bool ProcessBlocklist::isBlocked(const QString& name, const QString& cmdline, const QString& exe) const
{
    QString lowerName = name.toLower();

    // Direct name match
    if (m_blockedNames.contains(lowerName)) return true;

    // Check exe basename
    if (!exe.isEmpty()) {
        QString exeBase = exe.mid(exe.lastIndexOf('/') + 1).toLower();
        if (m_blockedNames.contains(exeBase)) return true;
    }

    // Check regex patterns against full cmdline
    for (const auto& pattern : m_patterns) {
        if (pattern.match(cmdline).hasMatch()) return true;
        if (pattern.match(exe).hasMatch()) return true;
    }

    return false;
}

void ProcessBlocklist::loadDefaults()
{
    // Screen capture
    for (const auto& name : {"obs", "obs-studio", "ffmpeg", "recordmydesktop",
                              "simplescreenrecorder", "kazam", "peek", "wf-recorder",
                              "vokoscreen", "screenstudio"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Screen sharing
    for (const auto& name : {"zoom", "teams", "discord", "slack", "skype",
                              "anydesk", "teamviewer", "rustdesk"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Messaging
    for (const auto& name : {"telegram-desktop", "signal-desktop", "pidgin",
                              "thunderbird", "evolution", "whatsapp"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Virtual machines
    for (const auto& name : {"virtualbox", "VBoxManage", "vmware", "vmplayer",
                              "qemu", "qemu-system-x86_64", "virt-manager",
                              "gnome-boxes"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Remote desktop
    for (const auto& name : {"xrdp", "vino", "remmina", "x11vnc", "tigervnc",
                              "vinagre", "krdc", "freerdp"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Terminals
    for (const auto& name : {"gnome-terminal", "konsole", "xterm", "alacritty",
                              "kitty", "tmux", "screen", "terminator", "tilix",
                              "guake", "yakuake", "urxvt", "rxvt", "st",
                              "xfce4-terminal", "lxterminal", "mate-terminal",
                              "foot", "wezterm"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Browsers (anything that's not us)
    for (const auto& name : {"firefox", "chromium", "chromium-browser", "brave",
                              "brave-browser", "vivaldi", "opera", "epiphany",
                              "midori", "falkon", "google-chrome", "microsoft-edge"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    // Automation tools
    for (const auto& name : {"xdotool", "xautomation", "ydotool", "wtype",
                              "xte", "xclip", "xsel", "wl-copy", "wl-paste"}) {
        m_blockedNames.insert(QString::fromLatin1(name));
    }

    qInfo() << "Loaded default blocklist:" << m_blockedNames.size() << "entries";
}

} // namespace openlock
