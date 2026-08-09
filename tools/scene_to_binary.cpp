#include "Scene/SceneBinaryFormat.h"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: scene_to_binary <input.json> [output.bscn]" << std::endl;
        return 1;
    }

    const std::string jsonPath = argv[1];
    const std::string binaryPath = (argc == 3)
        ? argv[2]
        : GameEngine::SceneBinaryFormat::jsonPathToBinaryPath(jsonPath);

    if (!GameEngine::SceneBinaryFormat::writeBinarySceneFromJsonFile(jsonPath, binaryPath)) {
        return 1;
    }

    std::cout << "Wrote binary scene: " << binaryPath << std::endl;
    return 0;
}
