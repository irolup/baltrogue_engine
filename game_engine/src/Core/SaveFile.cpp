#include "Core/SaveFile.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef VITA_BUILD
#include <filesystem>
#else
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#endif

namespace GameEngine {

bool SaveFile::ensureParentDirectory(const std::string& filePath) {
    const std::size_t slash = filePath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
        return true;
    }

    const std::string dir = filePath.substr(0, slash);
    if (dir.empty()) {
        return true;
    }

#ifdef VITA_BUILD
    std::string built;
    std::size_t start = 0;
    while (start < dir.size()) {
        const std::size_t next = dir.find('/', start);
        const std::string piece = (next == std::string::npos)
            ? dir.substr(start)
            : dir.substr(start, next - start);

        if (!piece.empty()) {
            if (!built.empty()) {
                built += '/';
            }
            built += piece;
            sceIoMkdir(built.c_str(), 0777);
        }

        if (next == std::string::npos) {
            break;
        }
        start = next + 1;
    }
    return true;
#else
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return !ec || std::filesystem::is_directory(dir);
#endif
}

bool SaveFile::loadFromFile(const std::string& path, std::string& outJson) {
    outJson.clear();

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    outJson = buffer.str();
    return true;
}

bool SaveFile::saveToFile(const std::string& path, const std::string& json) {
    if (!ensureParentDirectory(path)) {
        std::cerr << "SaveFile: Failed to create directory for " << path << std::endl;
        return false;
    }

    std::ofstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "SaveFile: Failed to write " << path << std::endl;
        return false;
    }

    file << json;
    return true;
}

}
