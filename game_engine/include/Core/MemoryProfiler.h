#ifndef MEMORY_PROFILER_H
#define MEMORY_PROFILER_H

#ifdef LINUX_BUILD

#include <vector>
#include <string>
#include <cstddef>

namespace GameEngine {

class Scene;

struct MemoryCategoryEntry {
    std::string name;
    size_t bytes;
    size_t count;
};

class MemoryProfiler {
public:
    static std::vector<MemoryCategoryEntry> getSummary(Scene* scene);
};

} // namespace GameEngine

#endif // LINUX_BUILD

#endif // MEMORY_PROFILER_H
