#ifndef SCENE_PICKER_H
#define SCENE_PICKER_H

#include "Core/Ray.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {

class Scene;
class SceneNode;

struct PickHit {
    PickHit();

    std::shared_ptr<SceneNode> node;
    glm::vec3 point;
    float distance;
    bool hit;
};

class ScenePicker {
public:
    static PickHit pickNode(Scene& scene, const Ray& ray, float maxDistance = 1000.0f);

    // Screen-space UI hit test, topmost first. SCREEN_SPACE text has no collider
    // and no world position a ray can reach, so menus need this rather than
    // pickNode() it is what drives mouse and touch menu selection
    static std::shared_ptr<SceneNode> hitTestScreen(Scene& scene, const glm::vec2& screenPoint,
                                                    const glm::vec2& viewportSize);

    static float getEmptyNodePickRadius() { return emptyNodePickRadius; }
    static void setEmptyNodePickRadius(float radius) { emptyNodePickRadius = radius; }

    static bool localBounds(SceneNode& node, glm::vec3& outMin, glm::vec3& outMax);

private:
    static void collectNodes(const std::shared_ptr<SceneNode>& node,
                             std::vector<std::shared_ptr<SceneNode>>& outNodes);

    static float emptyNodePickRadius;
};

}

#endif
