#include "Core/Project.h"
#include "Core/AssetPaths.h"

#include "../../vendor/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <sstream>

#ifdef VITA_BUILD
    #include <psp2/io/fcntl.h>
    #include <psp2/kernel/clib.h>
    #include <vector>
#else
    #include <filesystem>
    #include <fstream>
    #include <iostream>
#endif

namespace GameEngine {

const char* const Project::kFileName = "project.baltproj";
const char* const Project::kFileExtension = ".baltproj";
const char* const Project::kEngineVersion = "0.1.0";
const char* const Project::kLegacyBuildSettingsPath = "config/build_settings.txt";

Project& Project::getInstance() {
    static Project instance;
    return instance;
}

bool Project::readTextFile(const std::string& path, std::string& outText) {
#ifdef VITA_BUILD
    const SceUID fd = sceIoOpen(path.c_str(), SCE_O_RDONLY, 0777);
    if (fd < 0) {
        return false;
    }

    const SceOff size = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoLseek(fd, 0, SCE_SEEK_SET);
    if (size <= 0) {
        sceIoClose(fd);
        return false;
    }

    std::vector<char> bytes(static_cast<std::size_t>(size));
    const SceSSize read = sceIoRead(fd, bytes.data(), bytes.size());
    sceIoClose(fd);
    if (read <= 0) {
        return false;
    }

    outText.assign(bytes.data(), static_cast<std::size_t>(read));
    return true;
#else
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    outText = contents.str();
    return true;
#endif
}

void Project::logInfo(const std::string& message) {
#ifdef VITA_BUILD
    sceClibPrintf("%s\n", message.c_str());
#else
    std::cout << message << std::endl;
#endif
}

void Project::logError(const std::string& message) {
#ifdef VITA_BUILD
    sceClibPrintf("%s\n", message.c_str());
#else
    std::cerr << message << std::endl;
#endif
}

std::string Project::readString(const json& object, const char* key, const std::string& fallback) {
    const auto entry = object.find(key);
    if (entry == object.end() || !entry->is_string()) {
        return fallback;
    }
    return entry->get<std::string>();
}

int Project::readInt(const json& object, const char* key, int fallback) {
    const auto entry = object.find(key);
    if (entry == object.end() || !entry->is_number_integer()) {
        return fallback;
    }
    return entry->get<int>();
}

bool Project::readBool(const json& object, const char* key, bool fallback) {
    const auto entry = object.find(key);
    if (entry == object.end() || !entry->is_boolean()) {
        return fallback;
    }
    return entry->get<bool>();
}

std::string Project::directoryOf(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return "";
    }
    if (slash > 0 && path[slash - 1] == ':') {
        return path.substr(0, slash + 1);
    }
    return path.substr(0, slash);
}

void Project::resetToDefaults() {
    *this = Project();
}

void Project::applyPaths() const {
#ifndef VITA_BUILD
    // On the Vita everything is under app0:/
    AssetPaths::setProjectRoot(rootPath);
#endif
    AssetPaths::setAssetRoot(assetRoot);
}

bool Project::openDefault() {
    Project& project = getInstance();

#ifdef VITA_BUILD
    const std::string path = AssetPaths::resolve(kFileName);
#else
    const std::string path = findFromWorkingDirectory();
#endif

    if (!path.empty() && project.open(path)) {
        return true;
    }

    if (project.loadLegacyBuildSettings()) {
        logInfo(std::string("Project: no ") + kFileName + " found, using " +
                kLegacyBuildSettingsPath + ". Save from the editor to write one.");
        return true;
    }

    logError(std::string("Project: no ") + kFileName + " found, running with defaults.");
    return false;
}

bool Project::open(const std::string& path) {
    std::string file = path;
#ifdef LINUX_BUILD
    file = locate(path);
    if (file.empty()) {
        logError("Project: no " + std::string(kFileExtension) + " project file at " + path);
        return false;
    }
#endif

    std::string text;
    if (!readTextFile(file, text)) {
        logError("Project: could not read " + file);
        return false;
    }

    resetToDefaults();
    if (!parse(text)) {
        logError("Project: " + file + " is not valid project JSON");
        resetToDefaults();
        return false;
    }

    filePath = file;
    rootPath = directoryOf(file);
    applyPaths();

    if (engineVersion != kEngineVersion) {
        logInfo("Project: " + name + " was written by engine " + engineVersion + ", this is " + kEngineVersion);
    }
    return true;
}

bool Project::parse(const std::string& text) {
    const json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return false;
    }

    name = readString(root, "name", name);
    engineVersion = readString(root, "engineVersion", engineVersion);
    mainScene = readString(root, "mainScene", mainScene);
    assetRoot = readString(root, "assetRoot", assetRoot);
    inputMap = readString(root, "inputMap", inputMap);
    defaultInputMap = readString(root, "defaultInputMap", defaultInputMap);

    const auto build = root.find("build");
    if (build != root.end() && build->is_object()) {
        pc.title = readString(*build, "pc:title", pc.title);
        pc.executableName = readString(*build, "pc:executableName", pc.executableName);
        pc.windowWidth = readInt(*build, "pc:windowWidth", pc.windowWidth);
        pc.windowHeight = readInt(*build, "pc:windowHeight", pc.windowHeight);
        pc.fullscreen = readBool(*build, "pc:fullscreen", pc.fullscreen);
        pc.targetFrameRate = readInt(*build, "pc:targetFrameRate", pc.targetFrameRate);
        pc.renderer = (readString(*build, "pc:renderer", pc.renderer) == "opengl") ? "opengl" : "vulkan";

        vita.title = readString(*build, "vita:title", vita.title);
        vita.titleId = readString(*build, "vita:titleId", vita.titleId);
        vita.appVersion = readString(*build, "vita:appVersion", vita.appVersion);
        vita.vpkName = readString(*build, "vita:vpkName", vita.vpkName);
        vita.liveAreaStyle = readString(*build, "vita:style", vita.liveAreaStyle);
        vita.icon0Source = readString(*build, "vita:icon0", vita.icon0Source);
        vita.pic0Source = readString(*build, "vita:pic0", vita.pic0Source);
        vita.bg0Source = readString(*build, "vita:bg0", vita.bg0Source);
        vita.startupSource = readString(*build, "vita:startup", vita.startupSource);
    }

    const auto recent = root.find("recentScenes");
    if (recent != root.end() && recent->is_array()) {
        for (const json& entry : *recent) {
            if (entry.is_string() && recentScenes.size() < kMaxRecentScenes) {
                recentScenes.push_back(entry.get<std::string>());
            }
        }
    }

    return true;
}

std::string Project::trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

int Project::parsePositiveInt(const std::string& value, int fallback) {
    const int parsed = std::atoi(value.c_str());
    return (parsed > 0) ? parsed : fallback;
}

void Project::applyLegacyPcSetting(PcBuildSettings& pc, const std::string& key, const std::string& value) {
    if (key == "title") {
        pc.title = value;
    } else if (key == "executableName") {
        pc.executableName = value;
    } else if (key == "windowWidth") {
        pc.windowWidth = parsePositiveInt(value, pc.windowWidth);
    } else if (key == "windowHeight") {
        pc.windowHeight = parsePositiveInt(value, pc.windowHeight);
    } else if (key == "fullscreen") {
        pc.fullscreen = (value == "1" || value == "true");
    } else if (key == "targetFrameRate") {
        pc.targetFrameRate = std::atoi(value.c_str());
    } else if (key == "renderer") {
        pc.renderer = (value == "opengl") ? "opengl" : "vulkan";
    }
}

void Project::applyLegacyVitaSetting(VitaBuildSettings& vita, const std::string& key, const std::string& value) {
    if (key == "title") {
        vita.title = value;
    } else if (key == "titleId") {
        vita.titleId = value;
    } else if (key == "appVersion") {
        vita.appVersion = value;
    } else if (key == "vpkName") {
        vita.vpkName = value;
    } else if (key == "style") {
        vita.liveAreaStyle = value;
    } else if (key == "icon0") {
        vita.icon0Source = value;
    } else if (key == "pic0") {
        vita.pic0Source = value;
    } else if (key == "bg0") {
        vita.bg0Source = value;
    } else if (key == "startup") {
        vita.startupSource = value;
    }
}

bool Project::loadLegacyBuildSettings() {
    std::string text;
    if (!readTextFile(AssetPaths::resolve(kLegacyBuildSettingsPath), text)) {
        return false;
    }

    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::size_t equals = line.find('=', colon);
        if (equals == std::string::npos) {
            continue;
        }

        const std::string platform = line.substr(0, colon);
        const std::string key = trim(line.substr(colon + 1, equals - colon - 1));
        const std::string value = trim(line.substr(equals + 1));

        // A typo in the platform name must not quietly land on another one.
        if (platform == "pc") {
            applyLegacyPcSetting(pc, key, value);
        } else if (platform == "vita") {
            applyLegacyVitaSetting(vita, key, value);
        } else if (platform == "project" && key == "mainScene") {
            mainScene = value;
        }
    }

    name = pc.title;
    applyPaths();
    return true;
}

void Project::addRecentScene(const std::string& scenePath) {
    const std::string portable = AssetPaths::toPortable(scenePath);
    if (portable.empty()) {
        return;
    }

    for (std::size_t i = 0; i < recentScenes.size(); ++i) {
        if (recentScenes[i] == portable) {
            recentScenes.erase(recentScenes.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    recentScenes.insert(recentScenes.begin(), portable);
    if (recentScenes.size() > kMaxRecentScenes) {
        recentScenes.resize(kMaxRecentScenes);
    }
}

void Project::clearRecentScenes() {
    recentScenes.clear();
}

bool Project::hasShellUnsafeCharacters(const std::string& value) {
    return value.find_first_of("\"`$\\\n\r") != std::string::npos;
}

bool Project::isDigits(const std::string& value, std::size_t offset, std::size_t count) {
    for (std::size_t i = offset; i < offset + count; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

std::string Project::validateFileName(const char* label, const std::string& value) {
    if (value.empty()) {
        return std::string(label) + " is empty.";
    }
    if (hasShellUnsafeCharacters(value) || value.find_first_of("/ ") != std::string::npos) {
        return std::string(label) + " must be a plain file name, without a path or spaces.";
    }
    return "";
}

std::string Project::validateImageSource(const char* label, const std::string& path) {
    if (hasShellUnsafeCharacters(path)) {
        return std::string(label) + " path cannot contain quotes, backslashes or $.";
    }
    if (path.find(' ') != std::string::npos) {
        return std::string(label) + " path cannot contain spaces.";
    }
    return "";
}

std::string Project::validate() const {
    if (name.empty()) {
        return "Project name is empty.";
    }
    if (mainScene.empty()) {
        return "Main scene is empty.";
    }

    if (pc.title.empty()) {
        return "Game name is empty.";
    }
    const std::string executableError = validateFileName("Executable name", pc.executableName);
    if (!executableError.empty()) {
        return executableError;
    }

    if (vita.title.empty()) {
        return "Vita title is empty.";
    }
    if (hasShellUnsafeCharacters(vita.title)) {
        return "Vita title cannot contain quotes, backslashes or $.";
    }

    // vita-mksfoex writes TITLE_ID as a fixed nine byte field
    if (vita.titleId.size() != 9) {
        return "Title ID must be nine characters (four letters then five digits).";
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (!std::isupper(static_cast<unsigned char>(vita.titleId[i]))) {
            return "Title ID must start with four uppercase letters.";
        }
    }
    if (!isDigits(vita.titleId, 4, 5)) {
        return "Title ID must end with five digits.";
    }

    if (vita.appVersion.size() != 5 || vita.appVersion[2] != '.' ||
        !isDigits(vita.appVersion, 0, 2) || !isDigits(vita.appVersion, 3, 2)) {
        return "App version must look like 01.00.";
    }

    const std::string vpkError = validateFileName("VPK name", vita.vpkName);
    if (!vpkError.empty()) {
        return vpkError;
    }

    if (vita.liveAreaStyle != "a1" && vita.liveAreaStyle != "psmobile") {
        return "LiveArea style must be a1 or psmobile.";
    }

    const std::string sources[][2] = {
        {"Icon", vita.icon0Source},
        {"Loading screen", vita.pic0Source},
        {"Background", vita.bg0Source},
        {"Gate image", vita.startupSource}
    };
    for (const auto& source : sources) {
        const std::string error = validateImageSource(source[0].c_str(), source[1]);
        if (!error.empty()) {
            return error;
        }
    }

    return "";
}

#ifdef LINUX_BUILD

std::string Project::locate(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    std::error_code error;
    const std::filesystem::path candidate(path);

    if (std::filesystem::is_regular_file(candidate, error)) {
        return std::filesystem::absolute(candidate, error).lexically_normal().string();
    }

    if (!std::filesystem::is_directory(candidate, error)) {
        return "";
    }

    const std::filesystem::path conventional = candidate / kFileName;
    if (std::filesystem::is_regular_file(conventional, error)) {
        return std::filesystem::absolute(conventional, error).lexically_normal().string();
    }

    std::vector<std::string> found;
    for (const auto& entry : std::filesystem::directory_iterator(candidate, error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == kFileExtension) {
            found.push_back(entry.path().string());
        }
    }
    if (error || found.empty()) {
        return "";
    }

    // Deterministic rather than whatever order the filesystem hands back
    std::sort(found.begin(), found.end());
    if (found.size() > 1) {
        logInfo("Project: " + path + " holds " + std::to_string(found.size()) + " project files, opening " + found.front());
    }
    return std::filesystem::absolute(found.front(), error).lexically_normal().string();
}

std::string Project::fileNameForProject(const std::string& projectName) {
    std::string stem;
    for (const char character : projectName) {
        const unsigned char c = static_cast<unsigned char>(character);
        if (std::isalnum(c) || character == '_' || character == '-') {
            stem += character;
        }
    }
    if (stem.empty()) {
        return kFileName;
    }
    return stem + kFileExtension;
}

std::string Project::findFromWorkingDirectory() {
    std::error_code error;
    std::filesystem::path directory = std::filesystem::current_path(error);
    if (error) {
        return "";
    }

    // Stops at the filesystem root, whose parent_path() is itself
    for (std::filesystem::path previous; directory != previous; directory = directory.parent_path()) {
        previous = directory;
        const std::string found = locate(directory.string());
        if (!found.empty()) {
            return found;
        }
    }
    return "";
}

bool Project::save() {
    if (filePath.empty()) {
        logError("Project: no project file open to save to");
        return false;
    }
    return saveAs(filePath);
}

bool Project::saveAs(const std::string& projectFilePath) {
    // ordered_json, not json: the default object is a std::map, which would  alphabetise the file every save and bury the name under the build block
    using ordered_json = nlohmann::ordered_json;

    ordered_json build;
    build["pc:title"] = pc.title;
    build["pc:executableName"] = pc.executableName;
    build["pc:windowWidth"] = pc.windowWidth;
    build["pc:windowHeight"] = pc.windowHeight;
    build["pc:fullscreen"] = pc.fullscreen;
    build["pc:targetFrameRate"] = pc.targetFrameRate;
    build["pc:renderer"] = pc.renderer;
    build["vita:title"] = vita.title;
    build["vita:titleId"] = vita.titleId;
    build["vita:appVersion"] = vita.appVersion;
    build["vita:vpkName"] = vita.vpkName;
    build["vita:style"] = vita.liveAreaStyle;
    build["vita:icon0"] = vita.icon0Source;
    build["vita:pic0"] = vita.pic0Source;
    build["vita:bg0"] = vita.bg0Source;
    build["vita:startup"] = vita.startupSource;

    ordered_json root;
    root["name"] = name;
    root["engineVersion"] = std::string(kEngineVersion);
    root["mainScene"] = mainScene;
    root["assetRoot"] = assetRoot;
    root["inputMap"] = inputMap;
    root["defaultInputMap"] = defaultInputMap;
    root["build"] = build;
    root["recentScenes"] = recentScenes;

    std::ofstream file(projectFilePath);
    if (!file.is_open()) {
        logError("Project: could not write " + projectFilePath);
        return false;
    }
    // One key per line, which is what lets the Makefile and build_livearea.sh
    file << root.dump(4) << "\n";
    file.close();
    if (!file) {
        logError("Project: failed while writing " + projectFilePath);
        return false;
    }

    engineVersion = kEngineVersion;
    filePath = projectFilePath;
    rootPath = directoryOf(projectFilePath);
    applyPaths();
    return true;
}

bool Project::create(const std::string& directory, const std::string& projectName, std::string& outError) {
    outError.clear();

    if (directory.empty()) {
        outError = "No folder chosen.";
        return false;
    }
    if (projectName.empty()) {
        outError = "Project name is empty.";
        return false;
    }

    std::error_code error;
    const std::filesystem::path root(directory);
    const std::filesystem::path file = root / fileNameForProject(projectName);

    if (!locate(directory).empty()) {
        outError = directory + " already holds a project file.";
        return false;
    }
    if (std::filesystem::exists(file, error)) {
        outError = directory + " already holds a " + file.filename().string() + ".";
        return false;
    }

    std::filesystem::create_directories(root, error);
    if (error) {
        outError = "Could not create " + directory + ": " + error.message();
        return false;
    }

    // A project is expected to have these, without assets/scenes the editor's own Save Scene dialog has nowhere to point at
    const char* const folders[] = {
        "assets/scenes", "assets/textures", "assets/models", "assets/scripts",
        "assets/fonts", "assets/templates", "config"
    };
    for (const char* folder : folders) {
        std::filesystem::create_directories(root / folder, error);
        if (error) {
            outError = "Could not create " + (root / folder).string() + ": " + error.message();
            return false;
        }
    }

    Project fresh;
    fresh.name = projectName;
    fresh.pc.title = projectName;
    fresh.vita.title = projectName;
    if (!fresh.saveAs(file.lexically_normal().string())) {
        outError = "Could not write " + file.string();
        return false;
    }

    getInstance().applyPaths();
    return true;
}

#endif

}
