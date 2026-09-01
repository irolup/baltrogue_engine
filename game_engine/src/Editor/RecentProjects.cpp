#ifdef LINUX_BUILD

#include "Editor/RecentProjects.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace GameEngine {

std::string RecentProjects::getStorePath() {
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;

    if (configHome && configHome[0] != '\0') {
        base = configHome;
    } else {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') {
            return "";
        }
        base = std::filesystem::path(home) / ".config";
    }

    return (base / "baltrogue" / "recent_projects.txt").string();
}

std::vector<std::string> RecentProjects::load() {
    std::vector<std::string> entries;

    const std::string store = getStorePath();
    if (store.empty()) {
        return entries;
    }

    std::ifstream file(store);
    if (!file.is_open()) {
        return entries;
    }

    bool droppedAny = false;
    std::string line;
    std::error_code error;
    while (std::getline(file, line) && entries.size() < kMaxEntries) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!std::filesystem::is_regular_file(line, error)) {
            droppedAny = true;
            continue;
        }
        if (std::find(entries.begin(), entries.end(), line) == entries.end()) {
            entries.push_back(line);
        }
    }
    file.close();

    // Rewrite once rather than re-checking the same dead paths every session
    if (droppedAny) {
        save(entries);
    }
    return entries;
}

bool RecentProjects::save(const std::vector<std::string>& entries) {
    const std::string store = getStorePath();
    if (store.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(store).parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream file(store, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "# Projects opened in the Baltrogue editor, most recent first.\n";
    for (const std::string& entry : entries) {
        file << entry << "\n";
    }
    return true;
}

void RecentProjects::add(const std::string& projectFilePath) {
    if (projectFilePath.empty()) {
        return;
    }

    std::vector<std::string> entries = load();
    entries.erase(std::remove(entries.begin(), entries.end(), projectFilePath), entries.end());
    entries.insert(entries.begin(), projectFilePath);
    if (entries.size() > kMaxEntries) {
        entries.resize(kMaxEntries);
    }
    save(entries);
}

void RecentProjects::remove(const std::string& projectFilePath) {
    std::vector<std::string> entries = load();
    entries.erase(std::remove(entries.begin(), entries.end(), projectFilePath), entries.end());
    save(entries);
}

void RecentProjects::clear() {
    save({});
}

}

#endif
