#ifdef LINUX_BUILD

#include "Editor/ProjectAssets.h"
#include "Core/AssetPaths.h"
#include "Core/Project.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include <imgui.h>

namespace GameEngine {

std::string ProjectAssets::extensionOf(const std::string& path) {
    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

ProjectAssets::Kind ProjectAssets::classify(const std::string& path) {
    const std::string name = std::filesystem::path(path).filename().string();

    // Checked before the .json case below, which would otherwise swallow it
    if (name.size() > 14 && name.compare(name.size() - 14, 14, ".template.json") == 0) {
        return Kind::Template;
    }

    const std::string extension = extensionOf(path);

    if (extension == ".json" || extension == ".bscn") {
        return Kind::Scene;
    }
    if (extension == ".glb" || extension == ".gltf" || extension == ".obj") {
        return Kind::Model;
    }
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp") {
        return Kind::Texture;
    }
    if (extension == ".lua") {
        return Kind::Script;
    }
    if (extension == ".vert" || extension == ".frag" || extension == ".spv" || extension == ".comp" || extension == ".geom" || extension == ".cg") {
        return Kind::Shader;
    }
    if (extension == ".ttf" || extension == ".otf") {
        return Kind::Font;
    }
    if (extension == ".wav" || extension == ".ogg") {
        return Kind::Sound;
    }
    if (extension == ".txt" || extension == ".baltproj" || extension == ".mk" || extension == ".ini" || extension == ".bin") {
        return Kind::Data;
    }
    return Kind::Other;
}

const char* ProjectAssets::label(Kind kind) {
    switch (kind) {
        case Kind::Folder:
            return "Folders";
        case Kind::Scene:
            return "Scenes";
        case Kind::Template:
            return "Templates";
        case Kind::Model:
            return "Models";
        case Kind::Texture:
            return "Textures";
        case Kind::Script:
            return "Scripts";
        case Kind::Shader:
            return "Shaders";
        case Kind::Font:
            return "Fonts";
        case Kind::Sound:
            return "Sounds";
        case Kind::Data:
            return "Data";
        default:
            return "All";
    }
}

const char* ProjectAssets::tag(Kind kind) {
    switch (kind) {
        case Kind::Folder:
            return "[DIR]";
        case Kind::Scene:
            return "[SCN]";
        case Kind::Template:
            return "[TPL]";
        case Kind::Model:
            return "[MDL]";
        case Kind::Texture:
            return "[TEX]";
        case Kind::Script:
            return "[LUA]";
        case Kind::Shader:
            return "[SHD]";
        case Kind::Font:
            return "[FNT]";
        case Kind::Sound:
            return "[SND]";
        case Kind::Data:
            return "[DAT]";
        default:
            return "[   ]";
    }
}

const char* ProjectAssets::defaultFolder(Kind kind) {
    switch (kind) {
        case Kind::Scene:
            return "scenes";
        case Kind::Template:
            return "templates";
        case Kind::Model:
            return "models";
        case Kind::Texture:
            return "textures";
        case Kind::Script:
            return "scripts";
        case Kind::Shader:
            return "shaders";
        case Kind::Font:
            return "fonts";
        case Kind::Sound:
            return "sounds";
        default:
            return "imported";
    }
}

bool ProjectAssets::isInsideProject(const std::string& path) {
    const std::string& root = Project::getInstance().getRootPath();
    if (root.empty()) {
        return true;  // No project root means everything is relative to the cwd
    }

    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(std::filesystem::absolute(path, error), root, error);

    return !error && !relative.empty() && relative.native().rfind("..", 0) != 0;
}

std::string ProjectAssets::uniqueDestination(const std::string& absoluteDirectory, const std::string& fileName) {
    const std::filesystem::path directory(absoluteDirectory);
    const std::filesystem::path candidate = directory / fileName;

    std::error_code error;
    if (!std::filesystem::exists(candidate, error)) {
        return candidate.string();
    }

    const std::filesystem::path base = std::filesystem::path(fileName).stem();
    const std::string extension = std::filesystem::path(fileName).extension().string();
    for (int suffix = 1; suffix < 1000; ++suffix) {
        const std::filesystem::path attempt = directory / (base.string() + "_" + std::to_string(suffix) + extension);
        if (!std::filesystem::exists(attempt, error)) {
            return attempt.string();
        }
    }
    return candidate.string();
}

std::string ProjectAssets::importIntoProject(const std::string& sourcePath, std::string& outError) {
    outError.clear();

    if (sourcePath.empty()) {
        return "";
    }

    std::error_code error;
    if (!std::filesystem::exists(sourcePath, error)) {
        outError = sourcePath + " does not exist.";
        return "";
    }

    if (isInsideProject(sourcePath)) {
        return AssetPaths::toPortable(sourcePath);
    }

    const std::string& root = Project::getInstance().getRootPath();
    if (root.empty()) {
        outError = "No project is open, so there is nowhere to import into.";
        return "";
    }

    const Kind kind = classify(sourcePath);
    const std::filesystem::path destinationDirectory = std::filesystem::path(root) / Project::getInstance().assetRoot / defaultFolder(kind);

    std::filesystem::create_directories(destinationDirectory, error);
    if (error) {
        outError = "Could not create " + destinationDirectory.string() + ": " + error.message();
        return "";
    }

    const std::filesystem::path source(sourcePath);

    if (extensionOf(sourcePath) == ".gltf") {
        const std::filesystem::path sourceFolder = source.parent_path();
        const std::filesystem::path destinationFolder = destinationDirectory / sourceFolder.filename();

        std::filesystem::copy(sourceFolder, destinationFolder, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            outError = "Could not copy " + sourceFolder.string() + ": " + error.message();
            return "";
        }
        return AssetPaths::toPortable((destinationFolder / source.filename()).string());
    }

    const std::string destination = uniqueDestination(destinationDirectory.string(), source.filename().string());

    std::filesystem::copy_file(source, destination, error);
    if (error) {
        outError = "Could not copy " + sourcePath + ": " + error.message();
        return "";
    }

    return AssetPaths::toPortable(destination);
}

const char* ProjectAssets::dragDropPayloadId() {
    return "BALT_ASSET_PATH";
}

std::string ProjectAssets::acceptDrop(Kind expected) {
    const ImGuiPayload* preview = ImGui::GetDragDropPayload();
    if (!preview || !preview->IsDataType(dragDropPayloadId()) || preview->DataSize <= 0) {
        return "";
    }

    const std::string candidate(static_cast<const char*>(preview->Data), static_cast<std::size_t>(preview->DataSize) - 1);

    if (expected != Kind::Other && classify(candidate) != expected) {
        return "";
    }
    if (!ImGui::AcceptDragDropPayload(dragDropPayloadId())) {
        return "";
    }
    return candidate;
}

}

#endif
