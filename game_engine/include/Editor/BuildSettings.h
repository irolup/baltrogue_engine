#ifndef BUILD_SETTINGS_H
#define BUILD_SETTINGS_H

#ifdef LINUX_BUILD

#include <string>

namespace GameEngine {

static const char* const kBuildSettingsPath = "config/build_settings.txt";
static const char* const kLiveAreaScriptPath = "scripts/build_livearea.sh";

struct PcBuildSettings {
    std::string title = "Game Engine - Linux Game Build";
    std::string executableName = "Baltrogue";
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

struct BuildSettings {
    PcBuildSettings pc;
    VitaBuildSettings vita;

    bool load(const std::string& filepath = kBuildSettingsPath);
    bool save(const std::string& filepath = kBuildSettingsPath) const;

    std::string validate() const;

    static bool generateLiveAreaAssets(std::string& output);

    // ffmpeg and pngquant both on PATH the image conversion needs both.
    static bool converterToolsAvailable();
};

}

#endif
#endif
