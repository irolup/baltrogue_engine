#ifdef LINUX_BUILD

#include "Core/AssetPaths.h"
#include "Core/Engine.h"
#include "Core/Project.h"
#include "Editor/RecentProjects.h"
#include "Rendering/TextureManager.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

using namespace GameEngine;

int main(int argc, char** argv) {

    AssetPaths::initializeEngineRoot();
    Project& project = Project::getInstance();
    const std::string requested = (argc > 1) ? argv[1] : "";

    if (!requested.empty()) {
        if (!project.open(requested)) {
            std::cerr << "Could not open project: " << requested << std::endl;
            return -1;
        }
    } else {
        Project::openDefault();
    }

    if (project.isLoaded()) {
        RecentProjects::add(project.getFilePath());
        std::cout << "Project: " << project.name << " (" << project.getFilePath() << ")" << std::endl;
    }

    std::string restartInto;
    {
        Engine engine;

        if (!engine.initialize(EngineMode::EDITOR)) {
            std::cerr << "Failed to initialize game engine in editor mode!" << std::endl;
            return -1;
        }

        TextureManager::getInstance().discoverAllTextures("assets/textures");

        EditorSystem& editor = engine.getEditor();
        if (!editor.loadSceneFromFile(project.mainScene)) {
            std::cout << "Main scene " << project.mainScene << " not loaded, starting from an empty scene." << std::endl;
        }
        editor.updateWindowTitle();

        engine.run();

        restartInto = editor.getRequestedProjectRestart();
    }

    if (!restartInto.empty()) {
        std::vector<char*> arguments;
        arguments.push_back(argv[0]);
        arguments.push_back(const_cast<char*>(restartInto.c_str()));
        arguments.push_back(nullptr);

        execv(argv[0], arguments.data());
        std::cerr << "Could not restart the editor into " << restartInto << std::endl;
        return -1;
    }

    return 0;
}

#endif
