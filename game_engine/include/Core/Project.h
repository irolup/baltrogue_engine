#ifndef PROJECT_H
#define PROJECT_H

#include <string>
#include <vector>

#include "../../vendor/json/single_include/nlohmann/json_fwd.hpp"

namespace GameEngine {

struct PcBuildSettings {
    std::string title = "Game Engine - Linux Game Build";
    std::string executableName = "Baltrogue";

    int windowWidth = 960;
    int windowHeight = 544;
    bool fullscreen = false;

    int targetFrameRate = 60;

    std::string renderer = "vulkan";
};

struct VitaBuildSettings {
    std::string title = "Baltrogue";
    std::string titleId = "VSDK00420";
    std::string appVersion = "01.00";
    std::string vpkName = "Baltrogue";

    std::string liveAreaStyle = "a1";
    std::string icon0Source;
    std::string pic0Source;
    std::string bg0Source;
    std::string startupSource;
};

class Project {
public:

    static const char* const kFileName;
    static const char* const kFileExtension;
    static const char* const kEngineVersion;
    static const char* const kLegacyBuildSettingsPath;

    static Project& getInstance();

    static bool openDefault();

    bool open(const std::string& path);

    bool isLoaded() const { return !filePath.empty(); }
    const std::string& getFilePath() const { return filePath; }
    const std::string& getRootPath() const { return rootPath; }

    std::string name = "Untitled Project";
    std::string engineVersion = kEngineVersion;
    std::string mainScene = "assets/scenes/main_menu.json";
    std::string assetRoot = "assets";
    std::string inputMap = "config/input_mappings.txt";
    std::string defaultInputMap = "config/default_input_mappings.txt";

    PcBuildSettings pc;
    VitaBuildSettings vita;

    static const std::size_t kMaxRecentScenes = 10;
    const std::vector<std::string>& getRecentScenes() const { return recentScenes; }
    void addRecentScene(const std::string& scenePath);
    void clearRecentScenes();

    std::string validate() const;

#ifdef LINUX_BUILD
    bool save();
    bool saveAs(const std::string& projectFilePath);

    static bool create(const std::string& directory, const std::string& projectName, std::string& outError);

    static std::string locate(const std::string& path);
    static std::string fileNameForProject(const std::string& projectName);
    static std::string findFromWorkingDirectory();
#endif

private:
    void applyPaths() const;
    void resetToDefaults();
    bool parse(const std::string& text);
    bool loadLegacyBuildSettings();

    static bool readTextFile(const std::string& path, std::string& outText);
    static void logInfo(const std::string& message);
    static void logError(const std::string& message);


    static std::string readString(const nlohmann::json& object, const char* key, const std::string& fallback);

    static int readInt(const nlohmann::json& object, const char* key, int fallback);
    static bool readBool(const nlohmann::json& object, const char* key, bool fallback);

    static bool hasShellUnsafeCharacters(const std::string& value);
    static bool isDigits(const std::string& value, std::size_t offset, std::size_t count);
    static std::string validateFileName(const char* label, const std::string& value);
    static std::string validateImageSource(const char* label, const std::string& path);

    static std::string trim(const std::string& value);

    static int parsePositiveInt(const std::string& value, int fallback);

    static void applyLegacyPcSetting(PcBuildSettings& pc, const std::string& key, const std::string& value);
    static void applyLegacyVitaSetting(VitaBuildSettings& vita, const std::string& key, const std::string& value);

    static std::string directoryOf(const std::string& path);

    std::string filePath;
    std::string rootPath;
    std::vector<std::string> recentScenes;
};

}

#endif
