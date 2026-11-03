// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "integrity/DebugDetector.h"

#include <QFile>
#include <QTextStream>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

using namespace openlock;

namespace {

int tracerPid()
{
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return -1;
    }
    QTextStream in(&status);
    QString line;
    while (in.readLineInto(&line)) {
        if (line.startsWith(QLatin1String("TracerPid:"))) {
            return line.mid(10).trimmed().toInt();
        }
    }
    return -1;
}

} // namespace

// Regression: the detector used PTRACE_TRACEME, which makes the parent process our tracer
// for good (a tracee cannot detach itself). The next signal then stopped OpenLock forever.
TEST(DebugDetectorTest, CheckingDoesNotMakeTheProcessTraced)
{
    ASSERT_EQ(tracerPid(), 0) << "run this test without a debugger attached";

    DebugDetector detector;
    EXPECT_FALSE(detector.isBeingDebugged());

    EXPECT_EQ(tracerPid(), 0) << "isBeingDebugged() left the process traced";
}

// The observable failure of the old code: a child exiting delivers SIGCHLD, and a traced
// process stops on signal delivery until the tracer resumes it. Run the detector in a
// forked child, let it receive a signal, and require that it still exits normally.
TEST(DebugDetectorTest, ProcessKeepsRunningAfterSignalPostCheck)
{
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        DebugDetector detector;
        (void)detector.isBeingDebugged();
        std::signal(SIGUSR1, [](int) {});
        raise(SIGUSR1);
        _exit(tracerPid() == 0 ? 0 : 3);
    }

    int status = 0;
    pid_t waited = 0;
    for (int i = 0; i < 50; ++i) {  // up to 5 s
        waited = waitpid(child, &status, WNOHANG | WUNTRACED);
        if (waited != 0) {
            break;
        }
        usleep(100 * 1000);
    }
    if (waited == 0) {
        kill(child, SIGKILL);
        waitpid(child, &status, 0);
        FAIL() << "child did not finish within 5 s";
    }
    ASSERT_EQ(waited, child);
    if (WIFSTOPPED(status)) {
        const int stopSignal = WSTOPSIG(status);
        kill(child, SIGKILL);
        waitpid(child, &status, 0);
        FAIL() << "child entered a stop (signal " << stopSignal
               << ") after the detector ran: it was left traced";
    }
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}
