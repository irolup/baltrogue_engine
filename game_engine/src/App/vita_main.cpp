// Vita main — loads the main menu scene from JSON.

#include "Core/Engine.h"
#include "Rendering/TextureManager.h"
#include "Audio/AudioManager.h"
#include "Editor/BuildSettings.h"
#include <vitasdk.h>
#include <vitaGL.h>
#include <psp2/kernel/processmgr.h>

using namespace GameEngine;

int main() {

    vglInitExtended(0, VITA_WIDTH, VITA_HEIGHT, 0x1000000, SCE_GXM_MULTISAMPLE_NONE);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    Engine engine;

    if (!engine.initialize()) {
        sceClibPrintf("vita_main: Failed to initialize engine\n");
    } else {
        TextureManager::getInstance().discoverAllTextures("assets/textures");

        BuildSettings buildSettings;
        buildSettings.load();

        auto& sceneManager = engine.getSceneManager();
        if (sceneManager.loadSceneFromFile("Main Menu", buildSettings.mainScene)) {
            engine.run();
        } else {
            sceClibPrintf("vita_main: Failed to load main menu scene\n");
        }
    }
    AudioManager::getInstance().shutdown();

    vglEnd();

    sceKernelExitProcess(0);
    return 0;
}
