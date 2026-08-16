#include "Scene/ScenePicker.h"

#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/TextComponent.h"
#include "Rendering/Mesh.h"
#include "Rendering/Frustum.h"

#include <algorithm>

namespace GameEngine {

float ScenePicker::emptyNodePickRadius = 0.25f;

PickHit::PickHit()
    : node(nullptr)
    , point(0.0f)
    , distance(0.0f)
    , hit(false)
{
}

void ScenePicker::collectNodes(const std::shared_ptr<SceneNode>& node,
                               std::vector<std::shared_ptr<SceneNode>>& outNodes) {

    if (!node || !node->isActive() || !node->isVisible()) {
        return;
    }

    outNodes.push_back(node);
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        collectNodes(node->getChild(i), outNodes);
    }
}

bool ScenePicker::localBounds(SceneNode& node, glm::vec3& outMin, glm::vec3& outMax) {
    bool hasBounds = false;

    if (auto* meshRenderer = node.getComponent<MeshRenderer>()) {
        if (auto mesh = meshRenderer->getMesh()) {
            outMin = mesh->getBoundsMin();
            outMax = mesh->getBoundsMax();
            hasBounds = Frustum::areBoundsValid(outMin, outMax);
        }
    }

    if (auto* modelRenderer = node.getComponent<ModelRenderer>()) {
        const std::vector<std::shared_ptr<Mesh>>& meshes = modelRenderer->getMeshes();
        const std::vector<glm::mat4>& meshTransforms = modelRenderer->getMeshNodeTransforms();

        for (size_t i = 0; i < meshes.size(); ++i) {
            if (!meshes[i]) {
                continue;
            }

            const glm::vec3 meshMin = meshes[i]->getBoundsMin();
            const glm::vec3 meshMax = meshes[i]->getBoundsMax();
            if (!Frustum::areBoundsValid(meshMin, meshMax)) {
                continue;
            }

            const glm::mat4 meshTransform = (i < meshTransforms.size()) ? meshTransforms[i] : glm::mat4(1.0f);
            for (int corner = 0; corner < 8; ++corner) {
                const glm::vec3 localCorner(
                    (corner & 1) ? meshMax.x : meshMin.x,
                    (corner & 2) ? meshMax.y : meshMin.y,
                    (corner & 4) ? meshMax.z : meshMin.z);
                const glm::vec3 worldCorner = glm::vec3(meshTransform * glm::vec4(localCorner, 1.0f));

                if (!hasBounds) {
                    outMin = worldCorner;
                    outMax = worldCorner;
                    hasBounds = true;
                } else {
                    outMin = glm::min(outMin, worldCorner);
                    outMax = glm::max(outMax, worldCorner);
                }
            }
        }
    }

    if (!hasBounds) {
        outMin = glm::vec3(-emptyNodePickRadius);
        outMax = glm::vec3(emptyNodePickRadius);
    }

    return true;
}

PickHit ScenePicker::pickNode(Scene& scene, const Ray& ray, float maxDistance) {
    PickHit result;

    std::vector<std::shared_ptr<SceneNode>> nodes;
    collectNodes(scene.getRootNode(), nodes);

    for (const auto& node : nodes) {
        glm::vec3 boundsMin(0.0f);
        glm::vec3 boundsMax(0.0f);
        if (!localBounds(*node, boundsMin, boundsMax)) {
            continue;
        }

        float distance = 0.0f;
        if (!ray.intersectsOrientedBounds(boundsMin, boundsMax, node->getWorldMatrix(), distance)) {
            continue;
        }

        if (distance > maxDistance) {
            continue;
        }

        if (!result.hit || distance < result.distance) {
            result.hit = true;
            result.node = node;
            result.distance = distance;
            result.point = ray.pointAt(distance);
        }
    }

    return result;
}

std::shared_ptr<SceneNode> ScenePicker::hitTestScreen(Scene& scene, const glm::vec2& screenPoint,
                                                      const glm::vec2& viewportSize) {
    std::vector<std::shared_ptr<SceneNode>> nodes;
    collectNodes(scene.getRootNode(), nodes);

    std::shared_ptr<SceneNode> topmost = nullptr;

    for (const auto& node : nodes) {
        auto* textComponent = node->getComponent<TextComponent>();
        if (!textComponent || !textComponent->isEnabled()) {
            continue;
        }

        glm::vec4 rect(0.0f);
        if (!textComponent->getScreenRect(viewportSize, rect)) {
            continue;
        }

        const bool inside = screenPoint.x >= rect.x && screenPoint.x <= rect.x + rect.z &&
                            screenPoint.y >= rect.y && screenPoint.y <= rect.y + rect.w;
        if (inside) {
            topmost = node;
        }
    }

    return topmost;
}

}
