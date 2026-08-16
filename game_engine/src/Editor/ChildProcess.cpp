#ifdef LINUX_BUILD

#include "Editor/ChildProcess.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace GameEngine {

ChildProcess::ChildProcess()
    : processId(-1)
    , outputPipe(-1)
    , running(false)
    , exited(false)
    , exitCode(0)
{
}

ChildProcess::~ChildProcess() {
    stop();
    closePipe();
}

bool ChildProcess::start(const std::vector<std::string>& command) {
    if (running || command.empty()) {
        return false;
    }

    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) {
        std::cerr << "ChildProcess: pipe() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    const pid_t forked = fork();
    if (forked < 0) {
        std::cerr << "ChildProcess: fork() failed: " << std::strerror(errno) << std::endl;
        close(pipeEnds[0]);
        close(pipeEnds[1]);
        return false;
    }

    if (forked == 0) {
        close(pipeEnds[0]);
        dup2(pipeEnds[1], STDOUT_FILENO);
        dup2(pipeEnds[1], STDERR_FILENO);
        close(pipeEnds[1]);

        // Its own group, so stop() can signal the whole tree
        setpgid(0, 0);

        std::vector<char*> argv;
        argv.reserve(command.size() + 1);
        for (const std::string& argument : command) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        // Only reached when exec failed
        std::cerr << "ChildProcess: failed to run " << command[0] << ": " << std::strerror(errno) << std::endl;
        _exit(127);
    }

    close(pipeEnds[1]);
    outputPipe = pipeEnds[0];

    fcntl(outputPipe, F_SETFL, O_NONBLOCK);

    processId = forked;
    running = true;
    exited = false;
    exitCode = 0;
    pendingLine.clear();
    return true;
}

void ChildProcess::drainOutput() {
    if (outputPipe < 0) {
        return;
    }

    char buffer[4096];
    for (;;) {
        const ssize_t bytesRead = read(outputPipe, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            for (ssize_t i = 0; i < bytesRead; ++i) {
                const char c = buffer[i];
                if (c == '\n') {
                    flushPendingLine();
                } else if (c != '\r') {
                    pendingLine += c;
                }
            }
            continue;
        }

        break;
    }
}

void ChildProcess::flushPendingLine() {
    if (pendingLine.empty()) {
        return;
    }
    if (onOutput) {
        onOutput(pendingLine);
    }
    pendingLine.clear();
}

void ChildProcess::closePipe() {
    if (outputPipe >= 0) {
        close(outputPipe);
        outputPipe = -1;
    }
}

void ChildProcess::reap(bool blocking) {
    if (processId <= 0) {
        return;
    }

    int status = 0;
    const pid_t result = waitpid(processId, &status, blocking ? 0 : WNOHANG);
    if (result != processId) {
        return;
    }

    if (WIFEXITED(status)) {
        exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exitCode = -WTERMSIG(status);
    }

    running = false;
    exited = true;
    processId = -1;
}

void ChildProcess::update() {
    if (!running) {
        return;
    }

    drainOutput();
    reap(false);

    if (!running) {
        drainOutput();
        flushPendingLine();
        closePipe();
    }
}

void ChildProcess::stop(int graceMilliseconds) {
    if (!running || processId <= 0) {
        return;
    }

    kill(-processId, SIGTERM);

    const int pollIntervalMs = 20;
    for (int waited = 0; waited < graceMilliseconds; waited += pollIntervalMs) {
        drainOutput();
        reap(false);
        if (!running) {
            break;
        }
        usleep(pollIntervalMs * 1000);
    }

    if (running) {
        kill(-processId, SIGKILL);
        reap(true);
    }

    drainOutput();
    flushPendingLine();
    closePipe();
}

}

#endif
