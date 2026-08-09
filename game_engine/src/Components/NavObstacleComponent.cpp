#include "Components/NavObstacleComponent.h"
#include "Navigation/NavGrid.h"
#include "Navigation/NavGridRegistry.h"
#include "Scene/SceneNode.h"

namespace GameEngine {

NavObstacleComponent::NavObstacleComponent() {}

NavObstacleComponent::~NavObstacleComponent() {}

void NavObstacleComponent::start() {
    if (!owner) return;
    NavGridRegistry::get().forEachGrid([this](NavGrid* g) {
        g->registerObstacleNode(owner);
    });
}

void NavObstacleComponent::destroy() {
    if (!owner) return;
    NavGridRegistry::get().forEachGrid([this](NavGrid* g) {
        g->unregisterObstacleNode(owner);
    });
}

void NavObstacleComponent::suspend() {
    destroy();
}

void NavObstacleComponent::resume() {
    start();
}

void NavObstacleComponent::drawInspector() {
#if defined(EDITOR_BUILD)

#endif
}

} // namespace GameEngine
