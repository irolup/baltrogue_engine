// Vita main — loads the main menu scene from JSON.

#include "Core/Engine.h"
#include "Core/AssetPaths.h"
#include "Core/Project.h"
#include "Rendering/TextureManager.h"
#include "Audio/AudioManager.h"
#include <vitasdk.h>
#include <vitaGL.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>

using namespace GameEngine;

int main() {

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    vglSetupRuntimeShaderCompiler(SHARK_OPT_FAST, GL_TRUE, GL_FALSE, GL_TRUE);

    vglInitExtended(0, VITA_WIDTH, VITA_HEIGHT, 0x1000000, SCE_GXM_MULTISAMPLE_NONE);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    Project& project = Project::getInstance();
    Project::openDefault();

    Engine engine;

    if (!engine.initialize()) {
        sceClibPrintf("vita_main: Failed to initialize engine\n");
    } else {
        TextureManager::getInstance().discoverAllTextures("assets/textures");

        auto& sceneManager = engine.getSceneManager();
        if (sceneManager.loadSceneFromFile(AssetPaths::deriveSceneName(project.mainScene), project.mainScene)) {
            engine.run();
        } else {
            sceClibPrintf("vita_main: Failed to load main menu scene\n");
        }
    }
    AudioManager::getInstance().shutdown();

    sceKernelExitProcess(0);
    return 0;
}
