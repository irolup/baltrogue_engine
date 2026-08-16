#include "Scene/SceneBinaryFormat.h"

#include "Core/AssetPaths.h"

#include "../../vendor/json/single_include/nlohmann/json.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef VITA_BUILD
#include <vitasdk.h>
#endif

namespace GameEngine {

using json = nlohmann::json;

namespace {

struct SceneBinaryHeader {
    char magic[4];
    uint16_t version;
    uint16_t reserved;
};

bool hasExtension(const std::string& path, const char* extension) {
    const std::string ext = extension;
    if (path.length() < ext.length()) {
        return false;
    }
    return path.compare(path.length() - ext.length(), ext.length(), ext) == 0;
}

std::string replaceExtension(const std::string& path, const char* newExtension) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return path + newExtension;
    }
    return path.substr(0, dot) + newExtension;
}

#ifdef VITA_BUILD
bool readVitaFileBytes(const std::string& vitaPath, std::vector<uint8_t>& outBytes) {
    SceUID fd = sceIoOpen(vitaPath.c_str(), SCE_O_RDONLY, 0);
    if (fd < 0) {
        return false;
    }

    SceIoStat stat;
    if (sceIoGetstat(vitaPath.c_str(), &stat) < 0) {
        sceIoClose(fd);
        return false;
    }

    outBytes.resize(static_cast<size_t>(stat.st_size));
    const SceSSize bytesRead = sceIoRead(fd, outBytes.data(), stat.st_size);
    sceIoClose(fd);
    return bytesRead == stat.st_size;
}
#endif

} // namespace

std::string SceneBinaryFormat::jsonPathToBinaryPath(const std::string& jsonPath) {
    return replaceExtension(jsonPath, ".bscn");
}

std::string SceneBinaryFormat::resolveSceneLoadPath(const std::string& requestedPath) {
    if (hasExtension(requestedPath, ".bscn")) {
        return requestedPath;
    }

    if (!hasExtension(requestedPath, ".json")) {
        return requestedPath;
    }

    const std::string binaryPath = jsonPathToBinaryPath(requestedPath);
    if (AssetPaths::exists(AssetPaths::resolve(binaryPath))) {
        return binaryPath;
    }

    return requestedPath;
}

bool SceneBinaryFormat::isBinarySceneFile(const std::string& filepath) {
    return hasExtension(filepath, ".bscn");
}

bool SceneBinaryFormat::isBinaryPayload(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(SceneBinaryHeader)) {
        return false;
    }

    const SceneBinaryHeader* header = reinterpret_cast<const SceneBinaryHeader*>(data.data());
    return header->magic[0] == kMagic[0] &&
           header->magic[1] == kMagic[1] &&
           header->magic[2] == kMagic[2] &&
           header->magic[3] == kMagic[3] &&
           header->version == kVersion;
}

bool SceneBinaryFormat::readFileBytes(const std::string& filepath, std::vector<uint8_t>& outBytes) {
    outBytes.clear();

    const std::string resolvedPath = AssetPaths::resolve(filepath);

#ifdef VITA_BUILD
    return readVitaFileBytes(resolvedPath, outBytes);
#else
    std::ifstream file(resolvedPath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    if (size < 0) {
        return false;
    }

    file.seekg(0, std::ios::beg);
    outBytes.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(outBytes.data()), size)) {
        outBytes.clear();
        return false;
    }

    return true;
#endif
}

bool SceneBinaryFormat::encodeBinaryScene(const std::string& jsonText, std::vector<uint8_t>& outBytes) {
    outBytes.clear();

#ifdef VITA_BUILD
    json sceneJson = json::parse(jsonText.begin(), jsonText.end(), nullptr, false);
    if (sceneJson.is_discarded()) {
        return false;
    }
#else
    json sceneJson = json::parse(jsonText);
#endif

    const std::vector<uint8_t> payload = json::to_msgpack(sceneJson);

    SceneBinaryHeader header;
    header.magic[0] = kMagic[0];
    header.magic[1] = kMagic[1];
    header.magic[2] = kMagic[2];
    header.magic[3] = kMagic[3];
    header.version = kVersion;
    header.reserved = 0;

    outBytes.resize(sizeof(SceneBinaryHeader) + payload.size());
    std::memcpy(outBytes.data(), &header, sizeof(SceneBinaryHeader));
    if (!payload.empty()) {
        std::memcpy(outBytes.data() + sizeof(SceneBinaryHeader), payload.data(), payload.size());
    }

    return true;
}

bool SceneBinaryFormat::decodeBinaryScene(const std::vector<uint8_t>& data, std::string& outJsonText) {
    outJsonText.clear();

    if (!isBinaryPayload(data)) {
        return false;
    }

    const uint8_t* payloadStart = data.data() + sizeof(SceneBinaryHeader);
    const size_t payloadSize = data.size() - sizeof(SceneBinaryHeader);

#ifdef VITA_BUILD
    json sceneJson = json::from_msgpack(payloadStart, payloadStart + payloadSize, false, false);
    if (sceneJson.is_discarded()) {
        return false;
    }
#else
    json sceneJson = json::from_msgpack(payloadStart, payloadStart + payloadSize);
#endif

    outJsonText = sceneJson.dump();
    return true;
}

bool SceneBinaryFormat::writeBinarySceneFromJsonFile(const std::string& jsonPath, const std::string& binaryPath) {
    std::vector<uint8_t> jsonBytes;
    if (!readFileBytes(jsonPath, jsonBytes) || jsonBytes.empty()) {
        std::cerr << "SceneBinaryFormat: Failed to read JSON scene: " << jsonPath << std::endl;
        return false;
    }

    const std::string jsonText(jsonBytes.begin(), jsonBytes.end());
    std::vector<uint8_t> binaryBytes;
    if (!encodeBinaryScene(jsonText, binaryBytes)) {
        std::cerr << "SceneBinaryFormat: Failed to encode scene: " << jsonPath << std::endl;
        return false;
    }

    std::ofstream out(binaryPath.c_str(), std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "SceneBinaryFormat: Failed to write binary scene: " << binaryPath << std::endl;
        return false;
    }

    out.write(reinterpret_cast<const char*>(binaryBytes.data()), static_cast<std::streamsize>(binaryBytes.size()));
    return out.good();
}

}
