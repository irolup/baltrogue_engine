#include "Core/AssetPaths.h"

#include <vector>

#ifdef VITA_BUILD
    #include <psp2/io/stat.h>
#else
    #include <filesystem>
#endif

namespace GameEngine {

namespace {

const char* const kDevicePrefixes[] = {
    "app0:", "ux0:", "ur0:", "uma0:", "imc0:", "xmc0:", "vs0:", "vd0:"
};

const char* const kAssetDirectories[] = {
    "scenes", "textures", "models", "scripts", "shaders", "linux_shaders",
    "vulkan", "fonts", "sounds", "audio", "templates", "live_area"
};

const char* const kEngineDirectories[] = {
    "shaders", "linux_shaders", "vulkan", "fonts"
};

const char* const kDefaultAssetPrefix = "assets";

std::string lastSegment(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

#ifdef VITA_BUILD
const char* const kVitaRoot = "app0:/";
#endif

}

std::string AssetPaths::projectRoot;
std::string AssetPaths::assetRoot = "assets";
std::string AssetPaths::assetPrefix = "assets";
std::string AssetPaths::engineRoot;

void AssetPaths::setProjectRoot(const std::string& root) {
    projectRoot = normalize(root);
}

const std::string& AssetPaths::getProjectRoot() {
    return projectRoot;
}

void AssetPaths::setAssetRoot(const std::string& root) {
    assetRoot = root.empty() ? "assets" : normalize(root);
    assetPrefix = lastSegment(assetRoot);
}

const std::string& AssetPaths::getAssetRoot() {
    return assetRoot;
}

void AssetPaths::setEngineRoot(const std::string& root) {
    engineRoot = normalize(root);
}

const std::string& AssetPaths::getEngineRoot() {
    return engineRoot;
}

void AssetPaths::initializeEngineRoot() {
#ifndef VITA_BUILD
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error || !executable.has_parent_path()) {
        return;
    }

    const std::filesystem::path root = executable.parent_path().parent_path();
    if (!root.empty()) {
        setEngineRoot(root.lexically_normal().string());
    }
#endif
}

bool AssetPaths::hasDevicePrefix(const std::string& path) {
    for (const char* prefix : kDevicePrefixes) {
        const std::string device = prefix;
        if (path.compare(0, device.length(), device) == 0) {
            return true;
        }
    }
    return false;
}

bool AssetPaths::isAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

std::string AssetPaths::deriveSceneName(const std::string& scenePath) {
    size_t slash = scenePath.find_last_of("/\\");
    std::string filename = (slash == std::string::npos) ? scenePath : scenePath.substr(slash + 1);

    size_t dot = filename.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? filename : filename.substr(0, dot);

    return (stem == "main_menu") ? "Main Menu" : stem;
}

bool AssetPaths::exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
#ifdef VITA_BUILD
    SceIoStat stat;
    return sceIoGetstat(path.c_str(), &stat) >= 0;
#else
    std::error_code ec;
    return std::filesystem::exists(path, ec);
#endif
}

std::string AssetPaths::normalize(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::string prefix;
    std::string body = path;
    for (char& c : body) {
        if (c == '\\') {
            c = '/';
        }
    }

    const std::size_t colon = body.find(":/");
    if (colon != std::string::npos && hasDevicePrefix(body)) {
        prefix = body.substr(0, colon + 2);
        body = body.substr(colon + 2);
    } else if (!body.empty() && body[0] == '/') {
        prefix = "/";
        body = body.substr(1);
    }

    std::vector<std::string> segments;
    std::string segment;
    for (std::size_t i = 0; i <= body.length(); ++i) {
        const char c = (i < body.length()) ? body[i] : '/';
        if (c != '/') {
            segment += c;
            continue;
        }

        if (segment == "..") {
            if (!segments.empty() && segments.back() != "..") {
                segments.pop_back();
            } else if (prefix.empty()) {
                segments.push_back(segment);
            }
        } else if (!segment.empty() && segment != ".") {
            segments.push_back(segment);
        }
        segment.clear();
    }

    std::string result = prefix;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            result += '/';
        }
        result += segments[i];
    }
    return result;
}

std::string AssetPaths::join(const std::string& base, const std::string& path) {
    if (base.empty()) {
        return path;
    }
    if (path.empty()) {
        return base;
    }
    if (base[base.length() - 1] == '/') {
        return base + path;
    }
    return base + "/" + path;
}

std::string AssetPaths::firstSegment(const std::string& path) {
    const std::size_t slash = path.find('/');
    return (slash == std::string::npos) ? path : path.substr(0, slash);
}

std::string AssetPaths::dropFirstSegment(const std::string& path) {
    const std::size_t slash = path.find('/');
    return (slash == std::string::npos) ? std::string() : path.substr(slash + 1);
}

bool AssetPaths::isAssetDirectory(const std::string& segment) {
    for (const char* directory : kAssetDirectories) {
        if (segment == directory) {
            return true;
        }
    }
    return false;
}

bool AssetPaths::isEngineDirectory(const std::string& segment) {
    for (const char* directory : kEngineDirectories) {
        if (segment == directory) {
            return true;
        }
    }
    return false;
}

std::string AssetPaths::resolveAssetRelative(const std::string& path) {
    const std::string engineRelative = isEngineDirectory(firstSegment(path))
        ? join(kDefaultAssetPrefix, path)
        : std::string();

    return resolveProjectRelative(join(assetRoot, path), engineRelative);
}

std::string AssetPaths::resolveProjectRelative(const std::string& path, const std::string& engineRelative) {
    if (hasDevicePrefix(path) || isAbsolute(path)) {
        return path;
    }

#ifdef VITA_BUILD
    return kVitaRoot + path;
#else
    const std::string candidates[] = {
        join(projectRoot, path),
        engineRelative.empty() ? std::string() : join(engineRoot, engineRelative),
        path,
        join("..", path)
    };
    for (const std::string& candidate : candidates) {
        if (!candidate.empty() && exists(candidate)) {
            return candidate;
        }
    }
    return candidates[0];
#endif
}

std::string AssetPaths::resolve(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    const std::string normalized = normalize(path);
    if (hasDevicePrefix(normalized) || isAbsolute(normalized)) {
        return normalized;
    }

    const std::string segment = firstSegment(normalized);
    if (segment == assetPrefix || segment == kDefaultAssetPrefix) {
        return resolveAssetRelative(dropFirstSegment(normalized));
    }
    if (segment != "config" && isAssetDirectory(segment)) {
        return resolveAssetRelative(normalized);
    }
    return resolveProjectRelative(normalized, "");
}

std::string AssetPaths::resolveAsset(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    const std::string normalized = normalize(path);
    if (hasDevicePrefix(normalized) || isAbsolute(normalized)) {
        return normalized;
    }

    const std::string segment = firstSegment(normalized);
    if (segment == assetPrefix || segment == kDefaultAssetPrefix || segment == "config") {
        return resolve(normalized);
    }
    return resolveAssetRelative(normalized);
}

std::string AssetPaths::resolveTexture(const std::string& path) {
#ifdef VITA_BUILD
    if (path.empty() || hasDevicePrefix(path)) {
        return path;
    }

    const std::string portable = toPortable(path);
    const std::string texturesDir = assetPrefix + "/textures/";
    if (portable.compare(0, texturesDir.length(), texturesDir) == 0) {
        return kVitaRoot + lastSegment(portable);
    }
    return resolve(portable);
#else
    return resolve(path);
#endif
}

std::string AssetPaths::toPortable(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::string normalized = normalize(path);
    if (hasDevicePrefix(normalized)) {
        normalized = normalized.substr(normalized.find(":/") + 2);
    }

    const std::string markers[] = { assetPrefix + "/", "config/" };
    for (const std::string& marker : markers) {
        const std::size_t at = normalized.find(marker);
        if (at != std::string::npos) {
            return normalized.substr(at);
        }
    }

    if (!projectRoot.empty() && normalized.compare(0, projectRoot.length(), projectRoot) == 0) {
        std::string relative = normalized.substr(projectRoot.length());
        if (!relative.empty() && relative[0] == '/') {
            relative.erase(0, 1);
        }
        return relative;
    }

    return normalized;
}

}
