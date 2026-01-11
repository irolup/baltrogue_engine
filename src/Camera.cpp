#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera() 
    : position(0.0f, 0.0f, 7.0f)
    , front(0.0f, 0.0f, -1.0f)
    , up(0.0f, 1.0f, 0.0f)
    , right(1.0f, 0.0f, 0.0f)
    , worldUp(0.0f, 1.0f, 0.0f)
    , orientation(-90.0f, 0.0f, 0.0f)
    , sensitivity(DEFAULT_SENSITIVITY)
    , movementSpeed(DEFAULT_MOVEMENT_SPEED)
    , canFly(GL_FALSE) {
    updateCameraVectors();
}

Camera::Camera(const glm::vec3& position, const glm::vec3& orientation)
    : position(position)
    , front(0.0f, 0.0f, -1.0f)
    , up(0.0f, 1.0f, 0.0f)
    , right(1.0f, 0.0f, 0.0f)
    , worldUp(0.0f, 1.0f, 0.0f)
    , orientation(orientation)
    , sensitivity(DEFAULT_SENSITIVITY)
    , movementSpeed(DEFAULT_MOVEMENT_SPEED)
    , canFly(GL_FALSE) {
    updateCameraVectors();
}

void Camera::update() {
    updateCameraVectors();
}

void Camera::processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    orientation.x += xoffset;
    orientation.y -= yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        if (orientation.y > DEFAULT_PITCH_LIMIT)
            orientation.y = DEFAULT_PITCH_LIMIT;
        if (orientation.y < -DEFAULT_PITCH_LIMIT)
            orientation.y = -DEFAULT_PITCH_LIMIT;
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Camera::processKeyboard(int direction, float deltaTime) {
    float velocity = movementSpeed * deltaTime;
    
    if (direction == 0) // FORWARD
        position += (canFly ? front : glm::vec3(front.x, 0.0f, front.z)) * velocity;
    if (direction == 1) // BACKWARD
        position -= (canFly ? front : glm::vec3(front.x, 0.0f, front.z)) * velocity;
    if (direction == 2) // LEFT
        position -= right * velocity;
    if (direction == 3) // RIGHT
        position += right * velocity;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

void Camera::updateCameraVectors() {
    // Calculate the new Front vector
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(orientation.x)) * cos(glm::radians(orientation.y));
    newFront.y = sin(glm::radians(orientation.y));
    newFront.z = sin(glm::radians(orientation.x)) * cos(glm::radians(orientation.y));
    front = glm::normalize(newFront);
    
    // Also re-calculate the Right and Up vector
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}
