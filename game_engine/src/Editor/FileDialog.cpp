#ifdef LINUX_BUILD

#include "Editor/FileDialog.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace GameEngine {

std::string FileDialog::openFileDialog(const std::string& title, const std::string& filter) {
    ensureScenesDirectoryExists();
    
    std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" + 
                          getDefaultScenesDirectory() + " --file-filter=\"" + filter + " | " + filter + "\"";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open file dialog" << std::endl;
        return "";
    }
    
    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    
    // Remove trailing newline
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
    }
    
    return result;
}

std::string FileDialog::saveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultName) {
    ensureScenesDirectoryExists();
    
    std::string defaultPath = getDefaultScenesDirectory() + "/" + defaultName;
    std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" + 
                          defaultPath + " --save --file-filter=\"" + filter + " | " + filter + "\"";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open save dialog" << std::endl;
        return "";
    }
    
    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    
    // Remove trailing newline
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
    }
    
    return result;
}

bool FileDialog::isValidResult(const std::string& result) {
    return !result.empty() && result != "(null)";
}

std::string FileDialog::getDefaultScenesDirectory() {
    return "assets/scenes";
}

void FileDialog::ensureScenesDirectoryExists() {
    std::filesystem::path scenesDir(getDefaultScenesDirectory());
    if (!std::filesystem::exists(scenesDir)) {
        std::filesystem::create_directories(scenesDir);
        std::cout << "Created scenes directory: " << scenesDir << std::endl;
    }
}

} // namespace GameEngine

#endif // LINUX_BUILD
