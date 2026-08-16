#ifdef LINUX_BUILD

#include "Editor/EditorConsole.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <streambuf>

namespace GameEngine {

class EditorConsole::StreamTap : public std::streambuf {
public:
    StreamTap(std::ostream& stream, LogSeverity severity)
        : target(stream)
        , original(stream.rdbuf())
        , lineSeverity(severity)
    {
        target.rdbuf(this);
    }

    ~StreamTap() override {
        flushLine();
        target.rdbuf(original);
    }

protected:
    int overflow(int character) override {
        if (character == EOF) {
            return !EOF;
        }

        original->sputc(static_cast<char>(character));

        if (character == '\n') {
            flushLine();
        } else if (character != '\r') {
            pending += static_cast<char>(character);
        }
        return character;
    }

    std::streamsize xsputn(const char* data, std::streamsize count) override {
        for (std::streamsize i = 0; i < count; ++i) {
            overflow(static_cast<unsigned char>(data[i]));
        }
        return count;
    }

private:
    void flushLine() {
        if (pending.empty()) {
            return;
        }

        EditorConsole::getInstance().logProcessOutput(pending);
        pending.clear();
    }

    std::ostream& target;
    std::streambuf* original;
    LogSeverity lineSeverity;
    std::string pending;
};

EditorConsole::Entry::Entry(LogSeverity entrySeverity, const std::string& entryMessage)
    : severity(entrySeverity)
    , message(entryMessage)
{
}

EditorConsole::EditorConsole()
    : maxEntries(2000)
    , stdoutTap(nullptr)
    , stderrTap(nullptr)
{
    severityCounts[0] = 0;
    severityCounts[1] = 0;
    severityCounts[2] = 0;
}

EditorConsole::~EditorConsole() {
    removeStreamCapture();
}

EditorConsole& EditorConsole::getInstance() {
    static EditorConsole instance;
    return instance;
}

void EditorConsole::log(LogSeverity severity, const std::string& message) {
    entries.push_back(Entry(severity, message));
    ++severityCounts[static_cast<int>(severity)];
    trimToLimit();
}

void EditorConsole::logInfo(const std::string& message) {
    log(LogSeverity::Info, message);
}

void EditorConsole::logWarning(const std::string& message) {
    log(LogSeverity::Warning, message);
}

void EditorConsole::logError(const std::string& message) {
    log(LogSeverity::Error, message);
}

void EditorConsole::logProcessOutput(const std::string& line) {
    std::string lowered = line;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    LogSeverity severity = LogSeverity::Info;
    if (lowered.find("error") != std::string::npos || lowered.find("failed") != std::string::npos) {
        severity = LogSeverity::Error;
    } else if (lowered.find("warning") != std::string::npos) {
        severity = LogSeverity::Warning;
    }

    log(severity, line);
}

void EditorConsole::clear() {
    entries.clear();
    severityCounts[0] = 0;
    severityCounts[1] = 0;
    severityCounts[2] = 0;
}

size_t EditorConsole::getCount(LogSeverity severity) const {
    return severityCounts[static_cast<int>(severity)];
}

void EditorConsole::setMaxEntries(size_t limit) {
    maxEntries = (limit == 0) ? 1 : limit;
    trimToLimit();
}

void EditorConsole::trimToLimit() {
    if (entries.size() <= maxEntries) {
        return;
    }

    const size_t excess = entries.size() - maxEntries;
    for (size_t i = 0; i < excess; ++i) {
        --severityCounts[static_cast<int>(entries[i].severity)];
    }
    entries.erase(entries.begin(), entries.begin() + static_cast<long>(excess));
}

void EditorConsole::installStreamCapture() {
    if (!stdoutTap) {
        stdoutTap = new StreamTap(std::cout, LogSeverity::Info);
    }
    if (!stderrTap) {
        stderrTap = new StreamTap(std::cerr, LogSeverity::Error);
    }
}

void EditorConsole::removeStreamCapture() {
    delete stdoutTap;
    stdoutTap = nullptr;
    delete stderrTap;
    stderrTap = nullptr;
}

}

#endif
