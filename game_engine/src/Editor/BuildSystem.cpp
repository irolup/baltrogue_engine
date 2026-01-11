#ifdef LINUX_BUILD

#include "Editor/BuildSystem.h"
#include <iostream>

namespace GameEngine {

void BuildSystem::buildForLinux() {
    std::cout << "Building for Linux..." << std::endl;
    std::cout << "Using OpenGL 2.0 compatible shaders from assets/linux_shaders/" << std::endl;
    
    // Execute make command for Linux build
    int result = system("make linux");
    
    if (result == 0) {
        std::cout << "Linux build completed successfully!" << std::endl;
        std::cout << "Executable created at: build_linux/first_game" << std::endl;
    } else {
        std::cerr << "Linux build failed with error code: " << result << std::endl;
        std::cerr << "Check that all dependencies are installed: libglfw3-dev, libglew-dev, libpng-dev" << std::endl;
    }
}

void BuildSystem::buildForVita() {
    std::cout << "Building VPK for PS Vita..." << std::endl;
    std::cout << "Using VitaGL (OpenGL 2.0) with CG/HLSL shaders from assets/shaders/" << std::endl;
    
    // Execute make command for Vita build
    int result = system("make vita");
    
    if (result == 0) {
        std::cout << "Vita VPK build completed successfully!" << std::endl;
        std::cout << "VPK created at: build/first_game.vpk" << std::endl;
        std::cout << "Install on Vita using VitaShell or similar homebrew" << std::endl;
    } else {
        std::cerr << "Vita VPK build failed with error code: " << result << std::endl;
        std::cerr << "Check that VitaSDK is properly installed and configured" << std::endl;
        std::cerr << "Required: vitaGL, vitashark, and CG shader support" << std::endl;
    }
}

} // namespace GameEngine

#endif // LINUX_BUILD
