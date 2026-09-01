#ifdef LINUX_BUILD

#include "Core/Engine.h"
#include "Core/AssetPaths.h"
#include "Core/Project.h"
#include "Rendering/TextureManager.h"
#include <iostream>

using namespace GameEngine;

int main() {

    AssetPaths::initializeEngineRoot();
    Project& project = Project::getInstance();
    Project::openDefault();

    Engine engine;

    if (!engine.initialize()) {
        std::cerr << "Failed to initialize game engine!" << std::endl;
        return -1;
    }

    engine.setWindowTitle(project.pc.title);
    engine.setWindowSize(project.pc.windowWidth, project.pc.windowHeight);
    if (project.pc.fullscreen) {
        engine.setFullscreen(true);
    }
    engine.getTime().setTargetFrameRate(project.pc.targetFrameRate);

    engine.getInputManager().setEditorMode(true);

    TextureManager::getInstance().discoverAllTextures("assets/textures");

    auto& sceneManager = engine.getSceneManager();

    if (!sceneManager.loadSceneFromFile(AssetPaths::deriveSceneName(project.mainScene), project.mainScene)) {
        std::cerr << "Failed to load main scene: " << project.mainScene << std::endl;
        std::cerr << "Set it in the editor under Scene > Set as Main Scene." << std::endl;
        return -1;
    }

    std::cout << "Main scene loaded from " << project.mainScene << std::endl;

    engine.run();
    return 0;
}

#endif // LINUX_BUILD
