#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <glm/glm.hpp>

namespace GameEngine {

struct Frustum {
    // xyz = plane normal (normalized), w = distance from origin.
    glm::vec4 planes[6];

    Frustum();

    void update(const glm::mat4& viewProjection);

    bool containsAABB(const glm::vec3& boundsMin, const glm::vec3& boundsMax, const glm::mat4& transform) const;

    static bool areBoundsValid(const glm::vec3& boundsMin, const glm::vec3& boundsMax);
};

}

#endif
