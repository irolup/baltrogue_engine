#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace GameEngine {

class Transform {
public:
    Transform();
    Transform(const glm::vec3& position, const glm::quat& rotation = glm::quat(1, 0, 0, 0), const glm::vec3& scale = glm::vec3(1.0f));
    
    // Position
    const glm::vec3& getPosition() const { return position; }
    void setPosition(const glm::vec3& pos) { position = pos; markDirty(); }
    void translate(const glm::vec3& translation) { position += translation; markDirty(); }
    
    // Rotation (using quaternions for better interpolation and no gimbal lock)
    const glm::quat& getRotation() const { return rotation; }
    void setRotation(const glm::quat& rot) { rotation = rot; markDirty(); }
    void rotate(const glm::quat& rot) { rotation = rot * rotation; markDirty(); }
    
    // Euler angles (convenience methods)
    glm::vec3 getEulerAngles() const;
    void setEulerAngles(const glm::vec3& angles);
    void setEulerAngles(float x, float y, float z);
    
    // Scale
    const glm::vec3& getScale() const { return scale; }
    void setScale(const glm::vec3& s) { scale = s; markDirty(); }
    void setScale(float uniformScale) { scale = glm::vec3(uniformScale); markDirty(); }
    
    // Matrix operations
    glm::mat4 getMatrix() const;
    glm::mat3 getNormalMatrix() const;
    
    // Direction vectors
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    
    // Look at
    void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));
    
    // Reset
    void reset();
    
    // Dirty flag for optimization
    bool isDirty() const { return dirty; }
    void markClean() { dirty = false; }
    
private:
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    
    mutable bool dirty;
    mutable glm::mat4 cachedMatrix;
    
    void markDirty() { dirty = true; }
    void updateMatrix() const;
};

} // namespace GameEngine

#endif // TRANSFORM_H
