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

std::string FileDialog::getDefaultTemplatesDirectory() {
    return "assets/templates";
}

void FileDialog::ensureTemplatesDirectoryExists() {
    std::filesystem::path templatesDir(getDefaultTemplatesDirectory());
    if (!std::filesystem::exists(templatesDir)) {
        std::filesystem::create_directories(templatesDir);
        std::cout << "Created templates directory: " << templatesDir << std::endl;
    }
}

std::string FileDialog::openTemplateFileDialog(const std::string& title) {
    ensureTemplatesDirectoryExists();

    const std::string filter = "*.template.json";
    std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                          getDefaultTemplatesDirectory() + "/ --file-filter=\"" + filter + " | " + filter + "\"";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open template file dialog" << std::endl;
        return "";
    }

    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
    }

    return result;
}

std::string FileDialog::saveTemplateFileDialog(const std::string& title, const std::string& defaultName) {
    ensureTemplatesDirectoryExists();

    const std::string filter = "*.template.json";
    std::string defaultPath = getDefaultTemplatesDirectory() + "/" + defaultName;
    std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                          defaultPath + " --save --file-filter=\"" + filter + " | " + filter + "\"";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open template save dialog" << std::endl;
        return "";
    }

    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
    }

    return result;
}

std::string FileDialog::openImageFileDialog(const std::string& title) {
    const std::string filter = "*.png";
    std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=assets/" +
                          " --file-filter=\"" + filter + " | " + filter + "\"";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open image file dialog" << std::endl;
        return "";
    }

    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
    }

    return result;
}

std::string FileDialog::toProjectRelativePath(const std::string& path) {
    std::error_code error;
    std::filesystem::path relative = std::filesystem::relative(path, std::filesystem::current_path(), error);

    // Outside the project, so keep the absolute path the dialog returned
    if (error || relative.empty() || relative.native().rfind("..", 0) == 0) {
        return path;
    }

    return relative.string();
}

} // namespace GameEngine

#endif // LINUX_BUILD
