#ifndef BUILD_SYSTEM_H
#define BUILD_SYSTEM_H

#ifdef LINUX_BUILD

#include "Editor/ChildProcess.h"

#include <string>

namespace GameEngine {

class BuildSystem {
public:
    enum class Target {
        Linux,
        Vita
    };

    BuildSystem();
    ~BuildSystem();

    void update();

    bool startBuild(Target target);
    bool isBuilding() const;
    Target getBuildTarget() const { return buildTarget; }

    bool startGame();
    bool buildAndStartGame();
    bool isGameRunning() const;
    void stopGame();

    std::string getGameExecutablePath() const;
    bool gameExecutableExists() const;
    // True when a source file is newer than the binary
    bool isGameExecutableStale() const;

private:
    bool launchGameProcess();
    void onBuildFinished();

    ChildProcess buildProcess;
    ChildProcess gameProcess;

    Target buildTarget;

    bool startGameAfterBuild;
};

}

#endif
#endif
