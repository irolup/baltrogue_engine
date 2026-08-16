#include "Core/Ray.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace GameEngine {

Ray::Ray()
    : origin(0.0f)
    , direction(0.0f, 0.0f, -1.0f)
{
}

Ray::Ray(const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
    : origin(rayOrigin)
    , direction(0.0f, 0.0f, -1.0f)
{
    const float length = glm::length(rayDirection);
    if (length > 0.0f) {
        direction = rayDirection / length;
    }
}

glm::vec3 Ray::pointAt(float distance) const {
    return origin + direction * distance;
}

bool Ray::slabTest(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                   const glm::vec3& boundsMin, const glm::vec3& boundsMax, float& outDistance) {
    float nearestEntry = -std::numeric_limits<float>::max();
    float furthestExit = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(rayDirection[axis]) < 1e-8f) {
            // Parallel to this slab: only a miss when the origin is outside it.
            if (rayOrigin[axis] < boundsMin[axis] || rayOrigin[axis] > boundsMax[axis]) {
                return false;
            }
            continue;
        }

        const float inverseDirection = 1.0f / rayDirection[axis];
        float entry = (boundsMin[axis] - rayOrigin[axis]) * inverseDirection;
        float exit = (boundsMax[axis] - rayOrigin[axis]) * inverseDirection;
        if (entry > exit) {
            std::swap(entry, exit);
        }

        nearestEntry = std::max(nearestEntry, entry);
        furthestExit = std::min(furthestExit, exit);
        if (nearestEntry > furthestExit) {
            return false;
        }
    }

    if (furthestExit < 0.0f) {
        return false;
    }

    outDistance = std::max(nearestEntry, 0.0f);
    return true;
}

bool Ray::intersectsAABB(const glm::vec3& boundsMin, const glm::vec3& boundsMax, float& outDistance) const {
    return slabTest(origin, direction, boundsMin, boundsMax, outDistance);
}

bool Ray::intersectsOrientedBounds(const glm::vec3& boundsMin, const glm::vec3& boundsMax,
                                   const glm::mat4& transform, float& outDistance) const {
    const glm::mat4 inverseTransform = glm::inverse(transform);
    const glm::vec3 localOrigin = glm::vec3(inverseTransform * glm::vec4(origin, 1.0f));

    const glm::vec3 localDirection = glm::vec3(inverseTransform * glm::vec4(direction, 0.0f));

    return slabTest(localOrigin, localDirection, boundsMin, boundsMax, outDistance);
}

}
