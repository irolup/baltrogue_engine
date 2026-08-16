#ifndef RAY_H
#define RAY_H

#include <glm/glm.hpp>

namespace GameEngine {

// A world-space ray plus the intersection tests picking needs. Shared by the
// editor's click-to-select, the Lua pointer helpers and anything else that turns
// a screen point into something in the scene
class Ray {
public:
    Ray();
    Ray(const glm::vec3& origin, const glm::vec3& direction);

    const glm::vec3& getOrigin() const { return origin; }
    const glm::vec3& getDirection() const { return direction; }

    glm::vec3 pointAt(float distance) const;

    bool intersectsAABB(const glm::vec3& boundsMin, const glm::vec3& boundsMax, float& outDistance) const;

    bool intersectsOrientedBounds(const glm::vec3& boundsMin, const glm::vec3& boundsMax,
                                  const glm::mat4& transform, float& outDistance) const;

private:
    static bool slabTest(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                         const glm::vec3& boundsMin, const glm::vec3& boundsMax, float& outDistance);

    glm::vec3 origin;
    glm::vec3 direction;
};

}

#endif
