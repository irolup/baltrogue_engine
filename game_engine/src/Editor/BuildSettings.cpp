#include "Editor/BuildSettings.h"
#include "Core/AssetPaths.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace GameEngine {

namespace {

bool hasShellUnsafeCharacters(const std::string& value) {
    return value.find_first_of("\"`$\\\n\r") != std::string::npos;
}

std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

bool isDigits(const std::string& value, size_t offset, size_t count) {
    for (size_t i = offset; i < offset + count; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

// Unknown keys are ignored so a file written by a newer editor still loads
int parsePositiveInt(const std::string& value, int fallback) {
    const int parsed = std::atoi(value.c_str());
    return (parsed > 0) ? parsed : fallback;
}

void applyPcSetting(PcBuildSettings& pc, const std::string& key, const std::string& value) {
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

void applyVitaSetting(VitaBuildSettings& vita, const std::string& key, const std::string& value) {
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

std::string validateFileName(const char* label, const std::string& name) {
    if (name.empty()) {
        return std::string(label) + " is empty.";
    }
    if (hasShellUnsafeCharacters(name) || name.find_first_of("/ ") != std::string::npos) {
        return std::string(label) + " must be a plain file name, without a path or spaces.";
    }
    return "";
}

std::string validateImageSource(const char* label, const std::string& path) {
    if (hasShellUnsafeCharacters(path)) {
        return std::string(label) + " path cannot contain quotes, backslashes or $.";
    }
    // The build tracks the sources as make prerequisites, and make splits its
    // prerequisite lists on spaces.
    if (path.find(' ') != std::string::npos) {
        return std::string(label) + " path cannot contain spaces.";
    }
    return "";
}

void writePcBlock(std::ofstream& file, const PcBuildSettings& pc) {
    file << "pc:title=" << pc.title << "\n";
    file << "pc:executableName=" << pc.executableName << "\n";
    file << "pc:windowWidth=" << pc.windowWidth << "\n";
    file << "pc:windowHeight=" << pc.windowHeight << "\n";
    file << "pc:fullscreen=" << (pc.fullscreen ? "1" : "0") << "\n";
    file << "pc:targetFrameRate=" << pc.targetFrameRate << "\n";
    file << "pc:renderer=" << pc.renderer << "\n";
}

void writeVitaBlock(std::ofstream& file, const VitaBuildSettings& vita) {
    file << "vita:title=" << vita.title << "\n";
    file << "vita:titleId=" << vita.titleId << "\n";
    file << "vita:appVersion=" << vita.appVersion << "\n";
    file << "vita:vpkName=" << vita.vpkName << "\n";
    file << "vita:style=" << vita.liveAreaStyle << "\n";
    file << "vita:icon0=" << vita.icon0Source << "\n";
    file << "vita:pic0=" << vita.pic0Source << "\n";
    file << "vita:bg0=" << vita.bg0Source << "\n";
    file << "vita:startup=" << vita.startupSource << "\n";
}

} // namespace

bool BuildSettings::load(const std::string& filepath) {
    std::ifstream file(AssetPaths::resolve(filepath));
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const size_t equals = line.find('=', colon);
        if (equals == std::string::npos) {
            continue;
        }

        const std::string platform = line.substr(0, colon);
        const std::string key = trim(line.substr(colon + 1, equals - colon - 1));
        const std::string value = trim(line.substr(equals + 1));

        // A typo in the platform name must not quietly land on another one.
        if (platform == "pc") {
            applyPcSetting(pc, key, value);
        } else if (platform == "vita") {
            applyVitaSetting(vita, key, value);
        } else if (platform == "project" && key == "mainScene") {
            mainScene = value;
        }
    }

    return true;
}

bool BuildSettings::save(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "BuildSettings: could not write " << filepath << std::endl;
        return false;
    }

    file << "# Build Settings\n";
    file << "# Format: platform:key=value\n";
    file << "# Written by the editor (View > Build Settings). Each build reads its own block:\n";
    file << "#   pc   - window title of the Linux game, and build_linux/<executableName>\n";
    file << "#   vita - param.sfo, build/<vpkName>.vpk, and sce_sys/ built by " << kLiveAreaScriptPath << "\n";
    file << "# project - settings both platforms share, such as the scene they boot into.\n";
    file << "# An empty Vita image source ships the VPK without that asset.\n\n";

    file << "project:mainScene=" << mainScene << "\n\n";

    writePcBlock(file, pc);
    file << "\n";
    writeVitaBlock(file, vita);

    return true;
}

std::string BuildSettings::validate() const {
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
    for (size_t i = 0; i < 4; ++i) {
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
bool BuildSettings::generateLiveAreaAssets(std::string& output) {
    output.clear();

    const std::string command = std::string(kLiveAreaScriptPath) + " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        output = "Could not run " + std::string(kLiveAreaScriptPath);
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    return pclose(pipe) == 0;
}

bool BuildSettings::converterToolsAvailable() {
    return std::system("command -v ffmpeg > /dev/null 2>&1 && command -v pngquant > /dev/null 2>&1") == 0;
}
#endif

}
