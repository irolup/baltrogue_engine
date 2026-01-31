#ifdef LINUX_BUILD

#include "Core/MemoryProfiler.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Rendering/TextureManager.h"
#include "Rendering/FontManager.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include <algorithm>
#include <unordered_set>

namespace GameEngine {

namespace {

void collectMeshesAndMaterials(const std::shared_ptr<SceneNode>& node,
                               std::unordered_set<Mesh*>& seenMeshes,
                               std::unordered_set<Material*>& seenMaterials,
                               size_t& meshBytes, size_t& meshCount, size_t& materialCount) {
    if (!node) return;
    for (const auto& comp : node->getAllComponents()) {
        if (auto* mr = dynamic_cast<MeshRenderer*>(comp.get())) {
            auto mesh = mr->getMesh();
            if (mesh && seenMeshes.insert(mesh.get()).second) {
                meshBytes += mesh->getMemoryUsageBytes();
                ++meshCount;
            }
            auto mat = mr->getMaterial();
            if (mat && seenMaterials.insert(mat.get()).second) ++materialCount;
        }
        if (auto* modelR = dynamic_cast<ModelRenderer*>(comp.get())) {
            for (const auto& mesh : modelR->getMeshes()) {
                if (mesh && seenMeshes.insert(mesh.get()).second) {
                    meshBytes += mesh->getMemoryUsageBytes();
                    ++meshCount;
                }
            }
            for (const auto& mat : modelR->getMaterials()) {
                if (mat && seenMaterials.insert(mat.get()).second) ++materialCount;
            }
        }
    }
    for (size_t i = 0; i < node->getChildCount(); ++i)
        collectMeshesAndMaterials(node->getChild(i), seenMeshes, seenMaterials, meshBytes, meshCount, materialCount);
}

} // namespace

std::vector<MemoryCategoryEntry> MemoryProfiler::getSummary(Scene* scene) {
    std::vector<MemoryCategoryEntry> entries;
    auto& texMgr = TextureManager::getInstance();
    auto& fontMgr = FontManager::getInstance();

    entries.push_back({ "Textures", texMgr.getTotalTextureMemoryBytes(), texMgr.getTextureCount() });
    entries.push_back({ "Fonts", fontMgr.getTotalFontMemoryBytes(), fontMgr.getFontCount() });

    if (scene && scene->getRootNode()) {
        std::unordered_set<Mesh*> seenMeshes;
        std::unordered_set<Material*> seenMaterials;
        size_t meshBytes = 0, meshCount = 0, materialCount = 0;
        collectMeshesAndMaterials(scene->getRootNode(), seenMeshes, seenMaterials, meshBytes, meshCount, materialCount);
        entries.push_back({ "Meshes", meshBytes, meshCount });
        entries.push_back({ "Materials", materialCount * 512, materialCount }); // rough 512 bytes per material
        size_t nodeCount = scene->getNodeCount();
        entries.push_back({ "Scene nodes", nodeCount * 256, nodeCount }); // rough 256 bytes per node
    }

    std::sort(entries.begin(), entries.end(), [](const MemoryCategoryEntry& a, const MemoryCategoryEntry& b) {
        return a.bytes > b.bytes;
    });
    return entries;
}

} // namespace GameEngine

#endif // LINUX_BUILD
