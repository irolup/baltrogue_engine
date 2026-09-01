#ifdef LINUX_BUILD

#include "Editor/LiveAreaBuilder.h"

#include <cstdio>
#include <cstdlib>

namespace GameEngine {

bool LiveAreaBuilder::generateAssets(std::string& output) {
    output.clear();

    const std::string command = std::string(kLiveAreaScriptPath) + " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        output = "Could not run " + std::string(kLiveAreaScriptPath);
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    return pclose(pipe) == 0;
}

bool LiveAreaBuilder::converterToolsAvailable() {
    return std::system("command -v ffmpeg > /dev/null 2>&1 && command -v pngquant > /dev/null 2>&1") == 0;
}

}

#endif
