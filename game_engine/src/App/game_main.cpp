#ifdef LINUX_BUILD

#include "Core/Engine.h"
#include "Rendering/TextureManager.h"
#include "Editor/BuildSettings.h"
#include <iostream>

using namespace GameEngine;

int main() {
    Engine engine;

    if (!engine.initialize()) {
        std::cerr << "Failed to initialize game engine!" << std::endl;
        return -1;
    }

    BuildSettings buildSettings;
    buildSettings.load();

    engine.setWindowTitle(buildSettings.pc.title);
    engine.setWindowSize(buildSettings.pc.windowWidth, buildSettings.pc.windowHeight);
    if (buildSettings.pc.fullscreen) {
        engine.setFullscreen(true);
    }
    engine.getTime().setTargetFrameRate(buildSettings.pc.targetFrameRate);

    engine.getInputManager().setEditorMode(true);

    TextureManager::getInstance().discoverAllTextures("assets/textures");

    auto& sceneManager = engine.getSceneManager();
    if (!sceneManager.loadSceneFromFile("Main Menu", buildSettings.mainScene)) {
        std::cerr << "Failed to load main scene: " << buildSettings.mainScene << std::endl;
        std::cerr << "Set it in the editor under Scene > Set as Main Scene." << std::endl;
        return -1;
    }

    std::cout << "Main scene loaded from " << buildSettings.mainScene << std::endl;

    engine.run();
    return 0;
}

#endif // LINUX_BUILD
