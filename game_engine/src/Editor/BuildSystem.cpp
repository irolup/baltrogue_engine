#ifdef LINUX_BUILD

#include "Editor/BuildSystem.h"
#include "Core/Project.h"
#include "Editor/EditorConsole.h"

#include <filesystem>
#include <vector>
#include <iostream>
#include <thread>

namespace GameEngine {

BuildSystem::BuildSystem()
    : buildTarget(Target::Linux)
    , startGameAfterBuild(false)
{
    buildProcess.setOutputCallback([](const std::string& line) {
        EditorConsole::getInstance().logProcessOutput(line);
    });
    gameProcess.setOutputCallback([](const std::string& line) {
        EditorConsole::getInstance().logProcessOutput("[game] " + line);
    });
}

BuildSystem::~BuildSystem() {
    gameProcess.stop();
    buildProcess.stop();
}

std::string BuildSystem::getGameExecutablePath() const {
    const PcBuildSettings& pc = Project::getInstance().pc;
    const std::string directory = (pc.renderer == "opengl") ? "build_linux_gl/" : "build_linux/";
    return directory + pc.executableName;
}

bool BuildSystem::gameExecutableExists() const {
    std::error_code error;
    return std::filesystem::exists(getGameExecutablePath(), error);
}

bool BuildSystem::isGameExecutableStale() const {
    std::error_code error;
    const std::string executable = getGameExecutablePath();
    if (!std::filesystem::exists(executable, error)) {
        return true;
    }

    const auto executableTime = std::filesystem::last_write_time(executable, error);
    if (error) {
        return true;
    }

    const char* const watched[] = { "game_engine/src", "game_engine/include" };
    for (const char* directory : watched) {
        if (!std::filesystem::exists(directory, error)) {
            continue;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error)) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            const auto sourceTime = std::filesystem::last_write_time(entry.path(), error);
            if (!error && sourceTime > executableTime) {
                return true;
            }
        }
    }

    const auto makefileTime = std::filesystem::last_write_time("Makefile", error);
    return !error && makefileTime > executableTime;
}

bool BuildSystem::isBuilding() const {
    return buildProcess.isRunning();
}

bool BuildSystem::isGameRunning() const {
    return gameProcess.isRunning();
}

bool BuildSystem::startBuild(Target target) {
    if (isBuilding()) {
        EditorConsole::getInstance().logWarning("Build already running");
        return false;
    }

    buildTarget = target;

    const char* rule = (target == Target::Linux) ? "linux" : "vita";

    std::vector<std::string> command = { "make", rule };
    if (target == Target::Linux) {
        if (Project::getInstance().pc.renderer == "opengl") {
            command.push_back("USE_VULKAN=0");
        }
    }

    unsigned int jobs = std::thread::hardware_concurrency();
    if (jobs == 0) {
        jobs = 1;
    }
    command.push_back("-j" + std::to_string(jobs));

    std::string description;
    for (const std::string& part : command) {
        description += (description.empty() ? "" : " ") + part;
    }
    EditorConsole::getInstance().logInfo("Building: " + description);

    if (!buildProcess.start(command)) {
        EditorConsole::getInstance().logError("Failed to start make");
        startGameAfterBuild = false;
        return false;
    }

    return true;
}

bool BuildSystem::launchGameProcess() {
    const std::string executable = getGameExecutablePath();

    std::error_code error;
    if (!std::filesystem::exists(executable, error)) {
        EditorConsole::getInstance().logError("Game executable not found: " + executable);
        return false;
    }

    if (!gameProcess.start({ "./" + executable })) {
        EditorConsole::getInstance().logError("Failed to start " + executable);
        return false;
    }

    EditorConsole::getInstance().logInfo("Running " + executable);
    return true;
}

bool BuildSystem::startGame() {
    if (isGameRunning()) {
        EditorConsole::getInstance().logWarning("Game is already running");
        return false;
    }
    return launchGameProcess();
}

bool BuildSystem::buildAndStartGame() {
    if (isGameRunning()) {
        EditorConsole::getInstance().logWarning("Game is already running");
        return false;
    }

    if (!startBuild(Target::Linux)) {
        return false;
    }

    startGameAfterBuild = true;
    return true;
}

void BuildSystem::stopGame() {
    if (!isGameRunning()) {
        return;
    }

    EditorConsole::getInstance().logInfo("Stopping game");
    gameProcess.stop();
}

void BuildSystem::onBuildFinished() {
    const int exitCode = buildProcess.getExitCode();

    if (exitCode == 0) {
        EditorConsole::getInstance().logInfo("Build succeeded");

        if (buildTarget == Target::Vita) {
            EditorConsole::getInstance().logInfo("VPK: build/" + Project::getInstance().vita.vpkName + ".vpk");
        }

        if (startGameAfterBuild) {
            launchGameProcess();
        }
    } else {
        EditorConsole::getInstance().logError("Build failed with exit code " + std::to_string(exitCode));
    }

    startGameAfterBuild = false;
}

void BuildSystem::update() {
    const bool wasBuilding = buildProcess.isRunning();
    buildProcess.update();
    if (wasBuilding && !buildProcess.isRunning()) {
        onBuildFinished();
    }

    const bool wasRunning = gameProcess.isRunning();
    gameProcess.update();
    if (wasRunning && !gameProcess.isRunning()) {
        const int exitCode = gameProcess.getExitCode();
        if (exitCode == 0) {
            EditorConsole::getInstance().logInfo("Game exited");
        } else {
            EditorConsole::getInstance().logWarning("Game exited with code " + std::to_string(exitCode));
        }
    }
}

} // namespace GameEngine

#endif // LINUX_BUILD
