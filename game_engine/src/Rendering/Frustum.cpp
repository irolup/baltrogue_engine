#include "Rendering/Frustum.h"

#include <limits>

namespace GameEngine {

namespace {
// Corners are rejected slightly outside the plane so that objects straddling an
// edge are never popped out by floating point noise
const float kPlaneMargin = -0.1f;
const float kNormalizeEpsilon = 0.0001f;
}

Frustum::Frustum() {
    for (int i = 0; i < 6; ++i) {
        planes[i] = glm::vec4(0.0f);
    }
}

void Frustum::update(const glm::mat4& viewProjection) {
    const glm::mat4& m = viewProjection;

    // Each plane is a row of the matrix combined with the w row
    planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // left
    planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // right
    planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // bottom
    planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // top
    planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]); // near
    planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // far

    for (int i = 0; i < 6; ++i) {
        const float length = glm::length(glm::vec3(planes[i]));
        if (length > kNormalizeEpsilon) {
            planes[i] /= length;
        }
    }
}

bool Frustum::areBoundsValid(const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    if (boundsMin.x >= boundsMax.x || boundsMin.y >= boundsMax.y || boundsMin.z >= boundsMax.z) {
        return false;
    }

    const float maxVal = std::numeric_limits<float>::max();
    const float minVal = std::numeric_limits<float>::lowest();
    if (boundsMin.x > maxVal * 0.1f || boundsMax.x < minVal * 0.1f) {
        return false;
    }

    return true;
}

bool Frustum::containsAABB(const glm::vec3& boundsMin, const glm::vec3& boundsMax, const glm::mat4& transform) const {
    if (!areBoundsValid(boundsMin, boundsMax)) {
        return true;
    }

    const glm::vec3 corners[8] = {
        glm::vec3(transform * glm::vec4(boundsMin.x, boundsMin.y, boundsMin.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMax.x, boundsMin.y, boundsMin.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMin.x, boundsMax.y, boundsMin.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMax.x, boundsMax.y, boundsMin.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMin.x, boundsMin.y, boundsMax.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMax.x, boundsMin.y, boundsMax.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMin.x, boundsMax.y, boundsMax.z, 1.0f)),
        glm::vec3(transform * glm::vec4(boundsMax.x, boundsMax.y, boundsMax.z, 1.0f)),
    };

    for (int i = 0; i < 6; ++i) {
        const glm::vec3 normal(planes[i]);
        const float distance = planes[i].w;

        bool inside = false;
        for (int c = 0; c < 8; ++c) {
            if (glm::dot(normal, corners[c]) + distance > kPlaneMargin) {
                inside = true;
                break;
            }
        }

        if (!inside) {
            return false;
        }
    }

    return true;
}

}
