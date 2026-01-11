#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Platform.h"

class Camera {
public:
    Camera();
    Camera(const glm::vec3& position, const glm::vec3& orientation = glm::vec3(-90.0f, 0.0f, 0.0f));
    
    // Camera movement and control
    void update();
    void processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = GL_TRUE);
    void processKeyboard(int direction, float deltaTime);
    
    // Getters
    glm::mat4 getViewMatrix() const;
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    glm::vec3 getUp() const { return up; }
    glm::vec3 getRight() const { return right; }
    glm::vec3 getOrientation() const { return orientation; }
    GLboolean getFlyingMode() const { return canFly; }
    
    // Setters
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setOrientation(const glm::vec3& orient) { orientation = orient; }
    void setSensitivity(float sens) { sensitivity = sens; }
    void setMovementSpeed(float speed) { movementSpeed = speed; }
    void setFlyingMode(GLboolean fly) { canFly = fly; }
    
    // Camera properties
    static constexpr float DEFAULT_SENSITIVITY = 0.01f;
    static constexpr float DEFAULT_MOVEMENT_SPEED = 1.0f;
    static constexpr float DEFAULT_PITCH_LIMIT = 89.0f;

private:
    // Camera attributes
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    glm::vec3 orientation; // Yaw, Pitch, Roll
    
    // Camera options
    float sensitivity;
    float movementSpeed;
    GLboolean canFly;
    
    // Calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors();
};

#endif // CAMERA_H
