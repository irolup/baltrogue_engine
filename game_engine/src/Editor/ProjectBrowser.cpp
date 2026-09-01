#ifdef LINUX_BUILD

#include "Editor/ProjectBrowser.h"
#include "Editor/EditorConsole.h"
#include "Editor/EditorSystem.h"
#include "Editor/FileDialog.h"
#include "Core/AssetPaths.h"
#include "Core/Project.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <imgui.h>

namespace GameEngine {

ProjectBrowser::ProjectBrowser(EditorSystem& editorSystem)
    : editor(editorSystem)
{
}

bool ProjectBrowser::isSkippedDirectory(const std::string& name) {
    if (!name.empty() && name[0] == '.') {
        return true;
    }
    return name.compare(0, 5, "build") == 0 || name == "vendor" || name == "node_modules";
}

void ProjectBrowser::scan(const std::string& absoluteDirectory, Entry& into, int depth) {
    if (depth > kMaxDepth || entryCount >= kMaxEntries) {
        truncated = true;
        return;
    }

    std::error_code error;
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& item : std::filesystem::directory_iterator(absoluteDirectory, error)) {
        entries.push_back(item);
    }
    if (error) {
        return;
    }

    // Folders first, then files, each alphabetical
    std::sort(entries.begin(), entries.end(),
              [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                  std::error_code ignored;
                  const bool aDir = a.is_directory(ignored);
                  const bool bDir = b.is_directory(ignored);
                  if (aDir != bDir) {
                      return aDir;
                  }
                  return a.path().filename().string() < b.path().filename().string();
              });

    for (const auto& item : entries) {
        if (entryCount >= kMaxEntries) {
            truncated = true;
            return;
        }

        const std::string name = item.path().filename().string();
        const bool isDirectory = item.is_directory(error);

        if (isDirectory && isSkippedDirectory(name)) {
            continue;
        }
        if (!isDirectory && !name.empty() && name[0] == '.') {
            continue;
        }

        Entry child;
        child.name = name;
        child.absolutePath = item.path().string();
        child.portablePath = AssetPaths::toPortable(child.absolutePath);
        child.isDirectory = isDirectory;
        child.kind = isDirectory ? ProjectAssets::Kind::Folder : ProjectAssets::classify(child.absolutePath);
        ++entryCount;

        if (isDirectory) {
            scan(child.absolutePath, child, depth + 1);
        }
        into.children.push_back(std::move(child));
    }
}

void ProjectBrowser::refresh() {
    const Project& project = Project::getInstance();

    root = Entry();
    entryCount = 0;
    truncated = false;
    scanned = true;
    lastScanTime = ImGui::GetTime();

    scannedRoot = project.getRootPath();
    if (scannedRoot.empty()) {
        std::error_code error;
        scannedRoot = std::filesystem::current_path(error).string();
    }

    root.name = project.isLoaded() ? project.name : scannedRoot;
    root.absolutePath = scannedRoot;
    root.portablePath = "";
    root.isDirectory = true;
    root.kind = ProjectAssets::Kind::Folder;

    scan(scannedRoot, root, 0);
}

bool ProjectBrowser::matchesFilter(const Entry& entry) const {
    const bool hasSearch = searchBuffer[0] != '\0';
    const bool hasKind = kindFilter >= 0;

    if (!hasSearch && !hasKind) {
        return true;
    }

    if (!entry.isDirectory) {
        if (hasKind && static_cast<int>(entry.kind) != kindFilter) {
            return false;
        }
        if (hasSearch) {
            std::string lowered = entry.name;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::string needle = searchBuffer;
            std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lowered.find(needle) == std::string::npos) {
                return false;
            }
        }
        return true;
    }

    for (const Entry& child : entry.children) {
        if (matchesFilter(child)) {
            return true;
        }
    }
    return false;
}

void ProjectBrowser::openExternally(const std::string& absolutePath) {
    const std::string command = "xdg-open '" + absolutePath + "' &";
    const int result = std::system(command.c_str());
    (void)result;
}

void ProjectBrowser::openEntry(const Entry& entry) {
    if (entry.isDirectory) {
        return;
    }

    if (entry.kind == ProjectAssets::Kind::Scene) {
        if (entry.name.size() > 5 && entry.name.compare(entry.name.size() - 5, 5, ".bscn") == 0) {
            statusMessage = entry.name + " is compiled output. Open the .json instead.";
            statusIsError = true;
            return;
        }
        if (!editor.loadSceneFromFile(entry.portablePath)) {
            statusMessage = "Could not open " + entry.portablePath;
            statusIsError = true;
        }
        return;
    }

    if (entry.kind == ProjectAssets::Kind::Template) {
        auto scene = editor.getActiveScene();
        if (!scene) {
            statusMessage = "No scene open to insert into.";
            statusIsError = true;
            return;
        }
        auto parent = editor.getSelectedNode();
        if (!parent) {
            parent = scene->getRootNode();
        }
        if (!editor.instantiateTemplate(entry.portablePath, parent)) {
            statusMessage = "Could not insert " + entry.portablePath;
            statusIsError = true;
        }
        return;
    }

    openExternally(entry.absolutePath);
}

void ProjectBrowser::setEntryAsMainScene(const Entry& entry) {
    Project& project = Project::getInstance();
    project.mainScene = entry.portablePath;
    if (project.save()) {
        statusMessage = "Main scene set to " + project.mainScene;
        statusIsError = false;
        EditorConsole::getInstance().logInfo(statusMessage);
    } else {
        statusMessage = "Could not write " + std::string(Project::kFileName);
        statusIsError = true;
    }
}

void ProjectBrowser::importFromDialog() {
    const std::string picked = FileDialog::openFileDialog("Import Asset", "*");
    if (!FileDialog::isValidResult(picked)) {
        return;
    }

    std::string error;
    const std::string imported = ProjectAssets::importIntoProject(picked, error);
    if (imported.empty()) {
        statusMessage = error;
        statusIsError = true;
        return;
    }

    statusMessage = "Imported " + imported;
    statusIsError = false;
    EditorConsole::getInstance().logInfo(statusMessage);
    refresh();
}

void ProjectBrowser::drawToolbar() {
    if (ImGui::Button("Refresh")) {
        refresh();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rescan the project folder. Also happens on its own every few seconds.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Import...")) {
        importFromDialog();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copy a file from outside into the project, so what a scene stores is a path that travels.");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    const char* current = (kindFilter < 0) ? "All" : ProjectAssets::label(static_cast<ProjectAssets::Kind>(kindFilter));
    if (ImGui::BeginCombo("##KindFilter", current)) {
        if (ImGui::Selectable("All", kindFilter < 0)) {
            kindFilter = -1;
        }
        for (int i = 0; i < static_cast<int>(ProjectAssets::Kind::Count); ++i) {
            const ProjectAssets::Kind kind = static_cast<ProjectAssets::Kind>(i);
            if (kind == ProjectAssets::Kind::Folder) {
                continue;
            }
            if (ImGui::Selectable(ProjectAssets::label(kind), kindFilter == i)) {
                kindFilter = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##Search", "Search", searchBuffer, sizeof(searchBuffer));
}

void ProjectBrowser::drawEntryContextMenu(const Entry& entry) {
    if (!ImGui::BeginPopupContextItem()) {
        return;
    }

    if (!entry.isDirectory) {
        if (entry.kind == ProjectAssets::Kind::Scene && ImGui::MenuItem("Open Scene")) {
            openEntry(entry);
        }
        if (entry.kind == ProjectAssets::Kind::Template && ImGui::MenuItem("Insert Template")) {
            openEntry(entry);
        }
        if (entry.kind == ProjectAssets::Kind::Scene && ImGui::MenuItem("Set as Main Scene")) {
            setEntryAsMainScene(entry);
        }
        if (ImGui::MenuItem("Copy Path")) {
            ImGui::SetClipboardText(entry.portablePath.c_str());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", entry.portablePath.c_str());
        }
    }

    if (ImGui::MenuItem("Open Externally")) {
        openExternally(entry.absolutePath);
    }

    ImGui::Separator();

    if (entry.isDirectory && ImGui::MenuItem("New Folder...")) {
        pendingModal = "NewFolder";
        modalTargetPath = entry.absolutePath;
        nameBuffer[0] = '\0';
    }
    if (ImGui::MenuItem("Rename...")) {
        pendingModal = "Rename";
        modalTargetPath = entry.absolutePath;
        modalTargetName = entry.name;
        modalTargetIsDirectory = entry.isDirectory;
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", entry.name.c_str());
    }
    if (ImGui::MenuItem("Delete...")) {
        pendingModal = "Delete";
        modalTargetPath = entry.absolutePath;
        modalTargetName = entry.name;
        modalTargetIsDirectory = entry.isDirectory;
    }

    ImGui::EndPopup();
}

void ProjectBrowser::drawEntry(const Entry& entry) {
    if (!matchesFilter(entry)) {
        return;
    }

    ImGui::PushID(entry.absolutePath.c_str());

    if (entry.isDirectory) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;

        if (searchBuffer[0] != '\0' || kindFilter >= 0) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        const bool open = ImGui::TreeNodeEx("##dir", flags, "%s %s",  ProjectAssets::tag(entry.kind), entry.name.c_str());
        drawEntryContextMenu(entry);

        if (open) {
            for (const Entry& child : entry.children) {
                drawEntry(child);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        return;
    }

    const bool isSelected = (entry.portablePath == selectedPath);
    const bool isOpenScene = (entry.portablePath == editor.getActiveSceneFilePath());
    const bool isMainScene = (entry.portablePath == Project::getInstance().mainScene);

    std::string label = std::string(ProjectAssets::tag(entry.kind)) + " " + entry.name;
    if (isMainScene) {
        label += "   (main scene)";
    }
    if (isOpenScene) {
        label += "   (open)";
    }

    const bool tinted = isOpenScene || isMainScene;
    if (tinted) {
        ImGui::PushStyleColor(ImGuiCol_Text, isOpenScene ? ImVec4(0.4f, 0.8f, 1.0f, 1.0f) : ImVec4(0.9f, 0.8f, 0.3f, 1.0f));
    }

    if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
        selectedPath = entry.portablePath;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            openEntry(entry);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry.portablePath.c_str());
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(ProjectAssets::dragDropPayloadId(), entry.portablePath.c_str(), entry.portablePath.size() + 1);
        ImGui::Text("%s %s", ProjectAssets::tag(entry.kind), entry.name.c_str());
        ImGui::EndDragDropSource();
    }

    drawEntryContextMenu(entry);

    if (tinted) {
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}

void ProjectBrowser::drawModals() {
    if (!pendingModal.empty()) {
        ImGui::OpenPopup(pendingModal.c_str());
        pendingModal.clear();
    }

    std::error_code error;

    if (ImGui::BeginPopupModal("NewFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New folder in %s", modalTargetPath.c_str());
        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (nameBuffer[0] != '\0') {
                std::filesystem::create_directory(std::filesystem::path(modalTargetPath) / nameBuffer, error);
                if (error) {
                    statusMessage = "Could not create the folder: " + error.message();
                    statusIsError = true;
                } else {
                    refresh();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename %s", modalTargetName.c_str());
        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));

        const std::string portable = AssetPaths::toPortable(modalTargetPath);
        if (portable == editor.getActiveSceneFilePath()) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "This is the scene the editor has open. Save it first.");
        } else if (portable == Project::getInstance().mainScene) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "This is the project's main scene. Set it again afterwards.");
        }

        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            if (nameBuffer[0] != '\0' && modalTargetName != nameBuffer) {
                const std::filesystem::path source(modalTargetPath);
                std::filesystem::rename(source, source.parent_path() / nameBuffer, error);
                if (error) {
                    statusMessage = "Could not rename: " + error.message();
                    statusIsError = true;
                } else {
                    refresh();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete %s?", modalTargetName.c_str());
        ImGui::TextDisabled("%s", modalTargetPath.c_str());
        if (modalTargetIsDirectory) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "This deletes the folder and everything in it.");
        }
        ImGui::TextDisabled("There is no undo, and nothing checks what references it.");

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            const std::uintmax_t removed = std::filesystem::remove_all(modalTargetPath, error);
            if (error) {
                statusMessage = "Could not delete: " + error.message();
                statusIsError = true;
            } else {
                statusMessage = "Deleted " + modalTargetName + (modalTargetIsDirectory ? " (" + std::to_string(removed) + " entries)" : "");
                statusIsError = false;
                refresh();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ProjectBrowser::draw(bool* open) {
    ImGui::Begin("File Explorer", open);

    const Project& project = Project::getInstance();

    const std::string currentRoot = project.getRootPath();
    if (!scanned || (!currentRoot.empty() && currentRoot != scannedRoot)) {
        refresh();
    } else if (ImGui::GetTime() - lastScanTime > kRescanIntervalSeconds) {
        refresh();
    }

    drawToolbar();

    ImGui::TextDisabled("%s", root.absolutePath.c_str());
    if (truncated) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Tree truncated at %d entries.", kMaxEntries);
    }
    ImGui::Separator();

    ImGui::BeginChild("ProjectTree", ImVec2(0.0f, statusMessage.empty() ? 0.0f : -ImGui::GetFrameHeightWithSpacing()));
    for (const Entry& child : root.children) {
        drawEntry(child);
    }
    ImGui::EndChild();

    if (!statusMessage.empty()) {
        ImGui::Separator();
        if (statusIsError) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", statusMessage.c_str());
        } else {
            ImGui::TextDisabled("%s", statusMessage.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            statusMessage.clear();
        }
    }

    drawModals();

    ImGui::End();
}

}

#endif
