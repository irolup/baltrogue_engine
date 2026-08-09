#ifndef SCENE_BINARY_FORMAT_H
#define SCENE_BINARY_FORMAT_H

#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine {

class SceneBinaryFormat {
public:
    static constexpr char kMagic[4] = {'B', 'A', 'L', 'T'};
    static constexpr uint16_t kVersion = 1;

    static std::string resolveSceneLoadPath(const std::string& requestedPath);
    static std::string jsonPathToBinaryPath(const std::string& jsonPath);

    static bool isBinarySceneFile(const std::string& filepath);
    static bool isBinaryPayload(const std::vector<uint8_t>& data);

    static bool readFileBytes(const std::string& filepath, std::vector<uint8_t>& outBytes);
    static bool writeBinarySceneFromJsonFile(const std::string& jsonPath, const std::string& binaryPath);
    static bool decodeBinaryScene(const std::vector<uint8_t>& data, std::string& outJsonText);
    static bool encodeBinaryScene(const std::string& jsonText, std::vector<uint8_t>& outBytes);
};

}

#endif
