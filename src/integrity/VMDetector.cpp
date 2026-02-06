// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "integrity/VMDetector.h"

#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QNetworkInterface>

#ifdef __x86_64__
#include <cpuid.h>
#endif

namespace openlock {

VMDetector::VMDetector() = default;
VMDetector::~VMDetector() = default;

VMDetectionResult VMDetector::detect() const
{
    VMDetectionResult result;
    int checks = 0;
    int positives = 0;

    // Run all detection methods, accumulate confidence
    checks++;
    if (checkSystemdDetectVirt(result)) positives++;

    checks++;
    if (checkCPUID(result)) positives++;

    checks++;
    if (checkDMI(result)) positives++;

    checks++;
    if (checkScsiDevices(result)) positives++;

    checks++;
    if (checkMACAddress(result)) positives++;

    checks++;
    if (checkKernelModules(result)) positives++;

    checks++;
    if (checkProcCpuinfo(result)) positives++;

    // Any positive detection means VM
    if (positives > 0) {
        result.detected = true;
        result.confidenceScore = (positives * 100) / checks;
    }

    return result;
}

bool VMDetector::checkSystemdDetectVirt(VMDetectionResult& result) const
{
    QProcess proc;
    proc.start("systemd-detect-virt", QStringList());
    proc.waitForFinished(3000);

    QString output = proc.readAllStandardOutput().trimmed();

    if (proc.exitCode() == 0 && output != "none") {
        result.hypervisorName = output;
        qInfo() << "systemd-detect-virt:" << output;
        return true;
    }

    return false;
}

bool VMDetector::checkCPUID(VMDetectionResult& result) const
{
#ifdef __x86_64__
    unsigned int eax, ebx, ecx, edx;

    // Check hypervisor bit (CPUID leaf 1, ECX bit 31)
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (ecx & (1 << 31)) {
            // Hypervisor bit set — get hypervisor vendor string
            if (__get_cpuid(0x40000000, &eax, &ebx, &ecx, &edx)) {
                char vendor[13] = {};
                memcpy(vendor, &ebx, 4);
                memcpy(vendor + 4, &ecx, 4);
                memcpy(vendor + 8, &edx, 4);

                QString vendorStr = QString::fromLatin1(vendor).trimmed();
                if (!vendorStr.isEmpty()) {
                    if (result.hypervisorName.isEmpty()) {
                        result.hypervisorName = vendorStr;
                    }
                    qInfo() << "CPUID hypervisor vendor:" << vendorStr;
                }
            }
            return true;
        }
    }
#endif
    return false;
}

bool VMDetector::checkDMI(VMDetectionResult& result) const
{
    // Check DMI/SMBIOS strings for VM indicators
    QStringList dmiFiles = {
        "/sys/class/dmi/id/product_name",
        "/sys/class/dmi/id/sys_vendor",
        "/sys/class/dmi/id/board_vendor",
        "/sys/class/dmi/id/bios_vendor",
        "/sys/class/dmi/id/chassis_vendor"
    };

    QStringList vmIndicators = {
        "VirtualBox", "VMware", "QEMU", "Xen", "KVM",
        "Hyper-V", "Parallels", "Virtual Machine",
        "innotek GmbH", "Red Hat", "Bochs"
    };

    for (const QString& path : dmiFiles) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;

        QString content = QTextStream(&file).readLine().trimmed();
        for (const QString& indicator : vmIndicators) {
            if (content.contains(indicator, Qt::CaseInsensitive)) {
                if (result.hypervisorName.isEmpty()) {
                    result.hypervisorName = indicator;
                }
                qInfo() << "DMI VM indicator:" << content << "in" << path;
                return true;
            }
        }
    }

    return false;
}

bool VMDetector::checkScsiDevices(VMDetectionResult& result) const
{
    QFile file("/proc/scsi/scsi");
    if (!file.open(QIODevice::ReadOnly)) return false;

    QString content = file.readAll();
    QStringList vmScsi = {"VBOX", "VMware", "QEMU", "Virtual"};

    for (const QString& indicator : vmScsi) {
        if (content.contains(indicator, Qt::CaseInsensitive)) {
            if (result.hypervisorName.isEmpty()) {
                result.hypervisorName = indicator;
            }
            return true;
        }
    }

    return false;
}

bool VMDetector::checkMACAddress(VMDetectionResult& result) const
{
    // Known VM MAC address OUI prefixes
    QStringList vmOUIs = {
        "08:00:27",  // VirtualBox
        "00:0c:29",  // VMware
        "00:50:56",  // VMware
        "52:54:00",  // QEMU/KVM
        "00:16:3e",  // Xen
        "00:15:5d",  // Hyper-V
        "00:1c:42",  // Parallels
    };

    QMap<QString, QString> ouiToName = {
        {"08:00:27", "VirtualBox"},
        {"00:0c:29", "VMware"},
        {"00:50:56", "VMware"},
        {"52:54:00", "QEMU/KVM"},
        {"00:16:3e", "Xen"},
        {"00:15:5d", "Hyper-V"},
        {"00:1c:42", "Parallels"},
    };

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        QString mac = iface.hardwareAddress().toLower();
        QString prefix = mac.left(8);

        if (vmOUIs.contains(prefix)) {
            if (result.hypervisorName.isEmpty()) {
                result.hypervisorName = ouiToName.value(prefix, "Unknown VM");
            }
            qInfo() << "VM MAC detected:" << mac << "on" << iface.name();
            return true;
        }
    }

    return false;
}

bool VMDetector::checkKernelModules(VMDetectionResult& result) const
{
    QFile file("/proc/modules");
    if (!file.open(QIODevice::ReadOnly)) return false;

    QString modules = file.readAll();

    QMap<QString, QString> vmModules = {
        {"vboxguest", "VirtualBox"},
        {"vboxsf", "VirtualBox"},
        {"vboxvideo", "VirtualBox"},
        {"vmw_balloon", "VMware"},
        {"vmw_pvscsi", "VMware"},
        {"vmwgfx", "VMware"},
        {"vmw_vmci", "VMware"},
        {"virtio", "QEMU/KVM"},
        {"virtio_pci", "QEMU/KVM"},
        {"virtio_blk", "QEMU/KVM"},
        {"virtio_net", "QEMU/KVM"},
        {"xen_blkfront", "Xen"},
        {"xen_netfront", "Xen"},
        {"hv_vmbus", "Hyper-V"},
        {"hv_storvsc", "Hyper-V"},
    };

    for (auto it = vmModules.constBegin(); it != vmModules.constEnd(); ++it) {
        if (modules.contains(it.key())) {
            if (result.hypervisorName.isEmpty()) {
                result.hypervisorName = it.value();
            }
            qInfo() << "VM kernel module:" << it.key();
            return true;
        }
    }

    return false;
}

bool VMDetector::checkProcCpuinfo(VMDetectionResult& result) const
{
    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly)) return false;

    QTextStream stream(&file);
    QString line;
    while (stream.readLineInto(&line)) {
        if (line.startsWith("flags") && line.contains("hypervisor")) {
            qInfo() << "hypervisor flag found in /proc/cpuinfo";
            return true;
        }
    }

    Q_UNUSED(result);
    return false;
}

} // namespace openlock
