#include "Components/NavVolumeComponent.h"
#include "Navigation/NavGrid.h"
#include "Navigation/NavGridRegistry.h"
#include "Scene/SceneNode.h"

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

NavVolumeComponent::NavVolumeComponent()
    : gridSizeX_(32)
    , gridSizeZ_(32)
    , cellSize_(1.0f)
{}

NavVolumeComponent::~NavVolumeComponent() {
    if (grid_) {
        NavGridRegistry::get().removeGrid(grid_.get());
    }
}

void NavVolumeComponent::start() {
    if (!owner) return;
    if (!grid_) {
        grid_ = std::make_unique<NavGrid>();
        NavGridRegistry::get().addGrid(grid_.get());
    }
    syncToNavGrid();
}

void NavVolumeComponent::destroy() {
    if (grid_) {
        NavGridRegistry::get().removeGrid(grid_.get());
    }
}

void NavVolumeComponent::syncToNavGrid() {
    if (!owner) return;
    if (!grid_) {
        grid_ = std::make_unique<NavGrid>();
        NavGridRegistry::get().addGrid(grid_.get());
    }
    glm::vec3 origin = glm::vec3(owner->getWorldMatrix() * glm::vec4(owner->getTransform().getPosition(), 1.0f));
    grid_->setOrigin(origin);
    grid_->setGridSize(gridSizeX_, gridSizeZ_);
    grid_->setCellSize(cellSize_);
}

void NavVolumeComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::TreeNode("NavVolume")) {
        int gx = gridSizeX_, gz = gridSizeZ_;
        if (ImGui::DragInt("Grid Size X", &gx, 1, 1, 512)) setGridSizeX(gx);
        if (ImGui::DragInt("Grid Size Z", &gz, 1, 1, 512)) setGridSizeZ(gz);
        float cs = cellSize_;
        if (ImGui::DragFloat("Cell Size", &cs, 0.1f, 0.01f, 10.0f)) setCellSize(cs);
        ImGui::TreePop();
    }
#endif
}

} // namespace GameEngine
