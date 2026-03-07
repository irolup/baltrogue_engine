#include "Components/NavObstacleComponent.h"
#include "Navigation/NavGrid.h"
#include "Navigation/NavGridRegistry.h"
#include "Scene/SceneNode.h"

namespace GameEngine {

NavObstacleComponent::NavObstacleComponent() {}

NavObstacleComponent::~NavObstacleComponent() {}

void NavObstacleComponent::start() {
    if (!owner) return;
    gridsRegistered_.clear();
    NavGridRegistry::get().forEachGrid([this](NavGrid* g) {
        g->registerObstacleNode(owner);
        gridsRegistered_.push_back(g);
    });
}

void NavObstacleComponent::destroy() {
    if (!owner) return;
    for (NavGrid* g : gridsRegistered_)
        g->unregisterObstacleNode(owner);
    gridsRegistered_.clear();
}

void NavObstacleComponent::drawInspector() {
#if defined(EDITOR_BUILD)

#endif
}

} // namespace GameEngine
