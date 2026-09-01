#ifndef PROJECT_ASSETS_H
#define PROJECT_ASSETS_H

#ifdef LINUX_BUILD

#include <string>

namespace GameEngine {

class ProjectAssets {
public:
    enum class Kind {
        Folder,
        Scene,
        Template,
        Model,
        Texture,
        Script,
        Shader,
        Font,
        Sound,
        Data,
        Other,
        Count
    };

    static Kind classify(const std::string& path);

    static const char* label(Kind kind);
    static const char* tag(Kind kind);
    static const char* defaultFolder(Kind kind);

    static bool isInsideProject(const std::string& path);

    static std::string importIntoProject(const std::string& sourcePath, std::string& outError);

    static const char* dragDropPayloadId();

    static std::string acceptDrop(Kind expected);

private:
    static std::string extensionOf(const std::string& path);
    static std::string uniqueDestination(const std::string& absoluteDirectory, const std::string& fileName);
};

}

#endif
#endif
