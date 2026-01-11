#ifndef BUILD_SYSTEM_H
#define BUILD_SYSTEM_H

#ifdef LINUX_BUILD

namespace GameEngine {

class BuildSystem {
public:
    BuildSystem() = default;
    ~BuildSystem() = default;

    static void buildForLinux();
    static void buildForVita();
};

} // namespace GameEngine

#endif // LINUX_BUILD
#endif // BUILD_SYSTEM_H
