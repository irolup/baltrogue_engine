#ifndef CHILD_PROCESS_H
#define CHILD_PROCESS_H

#ifdef LINUX_BUILD

#include <functional>
#include <string>
#include <vector>
#include <sys/types.h>

namespace GameEngine {

// A child process the editor can start, read from and kill without ever blocking
class ChildProcess {
public:
    ChildProcess();
    ~ChildProcess();

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    using OutputCallback = std::function<void(const std::string& line)>;
    void setOutputCallback(OutputCallback callback) { onOutput = callback; }

    // command[0] is the executable, resolved on PATH
    bool start(const std::vector<std::string>& command);

    // Drains pending output and reaps the process once it exits
    void update();

    bool isRunning() const { return running; }

    bool hasExited() const { return exited; }
    int getExitCode() const { return exitCode; }

    void stop(int graceMilliseconds = 2000);

private:
    void drainOutput();
    void flushPendingLine();
    void closePipe();
    void reap(bool blocking);

    pid_t processId;
    int outputPipe;
    std::string pendingLine;
    bool running;
    bool exited;
    int exitCode;
    OutputCallback onOutput;
};

}

#endif
#endif
