#ifndef RECENT_PROJECTS_H
#define RECENT_PROJECTS_H

#ifdef LINUX_BUILD

#include <string>
#include <vector>

namespace GameEngine {

class RecentProjects {
public:
    static const std::size_t kMaxEntries = 10;

    static std::vector<std::string> load();

    static void add(const std::string& projectFilePath);
    static void remove(const std::string& projectFilePath);
    static void clear();

    static std::string getStorePath();

private:
    static bool save(const std::vector<std::string>& entries);
};

}

#endif
#endif
