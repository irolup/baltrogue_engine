#ifndef EDITOR_CONSOLE_H
#define EDITOR_CONSOLE_H

#ifdef LINUX_BUILD

#include <string>
#include <vector>

namespace GameEngine {

enum class LogSeverity {
    Info,
    Warning,
    Error
};

class EditorConsole {
public:
    static EditorConsole& getInstance();

    struct Entry {
        Entry(LogSeverity entrySeverity, const std::string& entryMessage);

        LogSeverity severity;
        std::string message;
    };

    void log(LogSeverity severity, const std::string& message);
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);

    void logProcessOutput(const std::string& line);

    void clear();

    const std::vector<Entry>& getEntries() const { return entries; }
    size_t getCount(LogSeverity severity) const;

    void setMaxEntries(size_t limit);
    size_t getMaxEntries() const { return maxEntries; }

    void installStreamCapture();
    void removeStreamCapture();

private:
    EditorConsole();
    ~EditorConsole();

    EditorConsole(const EditorConsole&) = delete;
    EditorConsole& operator=(const EditorConsole&) = delete;

    void trimToLimit();

    std::vector<Entry> entries;
    size_t maxEntries;
    size_t severityCounts[3];

    class StreamTap;
    StreamTap* stdoutTap;
    StreamTap* stderrTap;
};

}

#endif
#endif
