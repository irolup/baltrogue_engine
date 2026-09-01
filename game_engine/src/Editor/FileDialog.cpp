#ifdef LINUX_BUILD

#include "Editor/FileDialog.h"
#include "Core/AssetPaths.h"
#include "Core/Project.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace GameEngine {

std::string FileDialog::runDialog(const std::string& command, const char* what) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to open " << what << std::endl;
        return "";
    }

    char buffer[1024];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    // Remove trailing newline
    if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
    }

    return result;
}

std::string FileDialog::openFileDialog(const std::string& title, const std::string& filter) {
    ensureScenesDirectoryExists();

    const std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" + getDefaultScenesDirectory() + " --file-filter=\"" + filter + " | " + filter + "\"";
    return runDialog(command, "file dialog");
}

std::string FileDialog::saveFileDialog(const std::string& title, const std::string& filter, const std::string& defaultName) {
    ensureScenesDirectoryExists();

    const std::string defaultPath = getDefaultScenesDirectory() + "/" + defaultName;
    const std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                                defaultPath + " --save --file-filter=\"" + filter + " | " + filter + "\"";
    return runDialog(command, "save dialog");
}

bool FileDialog::isValidResult(const std::string& result) {
    return !result.empty() && result != "(null)";
}

std::string FileDialog::getDefaultScenesDirectory() {
    return AssetPaths::resolve("assets/scenes");
}

void FileDialog::ensureScenesDirectoryExists() {
    std::filesystem::path scenesDir(getDefaultScenesDirectory());
    if (!std::filesystem::exists(scenesDir)) {
        std::filesystem::create_directories(scenesDir);
        std::cout << "Created scenes directory: " << scenesDir << std::endl;
    }
}

std::string FileDialog::getDefaultTemplatesDirectory() {
    return AssetPaths::resolve("assets/templates");
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
    const std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                                getDefaultTemplatesDirectory() + "/ --file-filter=\"" + filter + " | " + filter + "\"";
    return runDialog(command, "template file dialog");
}

std::string FileDialog::saveTemplateFileDialog(const std::string& title, const std::string& defaultName) {
    ensureTemplatesDirectoryExists();

    const std::string filter = "*.template.json";
    const std::string defaultPath = getDefaultTemplatesDirectory() + "/" + defaultName;
    const std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                                defaultPath + " --save --file-filter=\"" + filter + " | " + filter + "\"";
    return runDialog(command, "template save dialog");
}

std::string FileDialog::openImageFileDialog(const std::string& title) {
    const std::string filter = "*.png";
    const std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                                AssetPaths::resolve(AssetPaths::getAssetRoot()) + "/" +
                                " --file-filter=\"" + filter + " | " + filter + "\"";
    return runDialog(command, "image file dialog");
}

std::string FileDialog::openProjectFileDialog(const std::string& title) {
    const std::string filter = "*.baltproj";
    const std::string& root = Project::getInstance().getRootPath();
    const std::string startDirectory = root.empty()
        ? std::filesystem::current_path().string()
        : std::filesystem::path(root).parent_path().string();

    const std::string command = "zenity --file-selection --title=\"" + title + "\" --filename=" +
                                startDirectory + "/ --file-filter=\"" + filter + " | " + filter + "\"";
    return runDialog(command, "project file dialog");
}

std::string FileDialog::openFolderDialog(const std::string& title) {
    const std::string& root = Project::getInstance().getRootPath();
    const std::string startDirectory = root.empty()
        ? std::filesystem::current_path().string()
        : std::filesystem::path(root).parent_path().string();

    const std::string command = "zenity --file-selection --directory --title=\"" + title +
                                "\" --filename=" + startDirectory + "/";
    return runDialog(command, "folder dialog");
}

std::string FileDialog::toProjectRelativePath(const std::string& path) {
    const std::string& projectRoot = Project::getInstance().getRootPath();
    const std::filesystem::path base = projectRoot.empty() ? std::filesystem::current_path()
                                                           : std::filesystem::path(projectRoot);

    std::error_code error;
    std::filesystem::path relative = std::filesystem::relative(path, base, error);

    // Outside the project, so keep the absolute path the dialog returned
    if (error || relative.empty() || relative.native().rfind("..", 0) == 0) {
        return path;
    }

    return relative.string();
}

} // namespace GameEngine

#endif // LINUX_BUILD
