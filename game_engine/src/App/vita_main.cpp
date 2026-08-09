// Vita main — loads the main menu scene from JSON.

#include "Core/Engine.h"
#include "Rendering/TextureManager.h"
#include <vitasdk.h>
#include <vitaGL.h>

using namespace GameEngine;

int main() {

    vglInitExtended(0, VITA_WIDTH, VITA_HEIGHT, 0x1000000, SCE_GXM_MULTISAMPLE_NONE);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    Engine engine;
    if (!engine.initialize()) {
        return -1;
    }

    TextureManager::getInstance().discoverAllTextures("assets/textures");

    auto& sceneManager = engine.getSceneManager();
    if (!sceneManager.loadSceneFromFile("Main Menu", "assets/scenes/main_menu.json")) {
#ifdef VITA_BUILD
        printf("vita_main: Failed to load main menu scene\n");
#endif
        return -1;
    }

    engine.run();
    vglEnd();
    return 0;
}
