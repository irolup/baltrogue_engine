#ifndef PROJECT_BROWSER_H
#define PROJECT_BROWSER_H

#ifdef LINUX_BUILD

#include <string>
#include <vector>

#include "Editor/ProjectAssets.h"

namespace GameEngine {

class EditorSystem;

class ProjectBrowser {
public:
    explicit ProjectBrowser(EditorSystem& editorSystem);

    void draw(bool* open);
    void refresh();

private:
    static const int kMaxEntries = 20000;
    static const int kMaxDepth = 12;
    static constexpr double kRescanIntervalSeconds = 3.0;

    struct Entry {
        std::string name;
        std::string portablePath;
        std::string absolutePath;
        ProjectAssets::Kind kind = ProjectAssets::Kind::Other;
        bool isDirectory = false;
        std::vector<Entry> children;
    };

    static bool isSkippedDirectory(const std::string& name);

    void scan(const std::string& absoluteDirectory, Entry& into, int depth);
    bool matchesFilter(const Entry& entry) const;

    void drawToolbar();
    void drawEntry(const Entry& entry);
    void drawEntryContextMenu(const Entry& entry);
    void drawModals();

    void openEntry(const Entry& entry);
    void importFromDialog();
    void setEntryAsMainScene(const Entry& entry);
    static void openExternally(const std::string& absolutePath);

    EditorSystem& editor;
    Entry root;

    double lastScanTime = 0.0;
    bool scanned = false;
    int entryCount = 0;
    bool truncated = false;
    std::string scannedRoot;

    char searchBuffer[64] = "";
    int kindFilter = -1;
    std::string selectedPath;

    std::string pendingModal;
    std::string modalTargetPath;
    std::string modalTargetName;
    bool modalTargetIsDirectory = false;
    char nameBuffer[128] = "";
    std::string statusMessage;
    bool statusIsError = false;
};

}

#endif
#endif
