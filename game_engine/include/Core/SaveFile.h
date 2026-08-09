#ifndef SAVE_FILE_H
#define SAVE_FILE_H

#include <string>

namespace GameEngine {

class SaveFile {
public:
    static bool loadFromFile(const std::string& path, std::string& outJson);
    static bool saveToFile(const std::string& path, const std::string& json);

private:
    static bool ensureParentDirectory(const std::string& filePath);
};

}

#endif
