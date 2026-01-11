#include "Core/Transform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>

namespace GameEngine {

Transform::Transform()
    : position(0.0f)
    , rotation(1.0f, 0.0f, 0.0f, 0.0f) // Identity quaternion
    , scale(1.0f)
    , dirty(true)
    , cachedMatrix(1.0f)
{
}

Transform::Transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& s)
    : position(pos)
    , rotation(rot)
    , scale(s)
    , dirty(true)
    , cachedMatrix(1.0f)
{
}

glm::vec3 Transform::getEulerAngles() const {
    return glm::degrees(glm::eulerAngles(rotation));
}

void Transform::setEulerAngles(const glm::vec3& angles) {
    glm::vec3 radians = glm::radians(angles);
    rotation = glm::quat(radians);
    markDirty();
}

void Transform::setEulerAngles(float x, float y, float z) {
    setEulerAngles(glm::vec3(x, y, z));
}

glm::mat4 Transform::getMatrix() const {
    if (dirty) {
        updateMatrix();
    }
    return cachedMatrix;
}

glm::mat3 Transform::getNormalMatrix() const {
    glm::mat4 modelMatrix = getMatrix();
    return glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
}

glm::vec3 Transform::getForward() const {
    return rotation * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 Transform::getRight() const {
    return rotation * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 Transform::getUp() const {
    return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
}

void Transform::lookAt(const glm::vec3& target, const glm::vec3& up) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 actualUp = glm::cross(right, forward);
    
    glm::mat3 rotationMatrix;
    rotationMatrix[0] = right;
    rotationMatrix[1] = actualUp;
    rotationMatrix[2] = -forward;
    
    rotation = glm::quat_cast(rotationMatrix);
    markDirty();
}

void Transform::reset() {
    position = glm::vec3(0.0f);
    rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    scale = glm::vec3(1.0f);
    markDirty();
}

void Transform::updateMatrix() const {
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);
    
    cachedMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    dirty = false;
}

} // namespace GameEngine
