#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace GameEngine {

class Mesh;
class Material;

enum class ProjectionType {
    PERSPECTIVE,
    ORTHOGRAPHIC
};

class CameraComponent : public Component {
public:
    CameraComponent();
    virtual ~CameraComponent();
    
    COMPONENT_TYPE(CameraComponent)
    
    virtual void update(float deltaTime) override;
    
    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix();
    glm::mat4 getViewProjectionMatrix();
    
    ProjectionType getProjectionType() const { return projectionType; }
    void setProjectionType(ProjectionType type) { projectionType = type; updateProjection(); }
    
    float getFOV() const { return fov; }
    void setFOV(float newFov) { fov = newFov; updateProjection(); }
    
    float getAspectRatio() const { return aspectRatio; }
    void setAspectRatio(float ratio) { aspectRatio = ratio; updateProjection(); }
    
    float getNearPlane() const { return nearPlane; }
    void setNearPlane(float near) { nearPlane = near; updateProjection(); }
    
    float getFarPlane() const { return farPlane; }
    void setFarPlane(float far) { farPlane = far; updateProjection(); }
    
    float getOrthographicSize() const { return orthographicSize; }
    void setOrthographicSize(float size) { orthographicSize = size; updateProjection(); }
    
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    
    bool isActive() const { return isActiveCamera; }
    void setActive(bool active) { isActiveCamera = active; }
    
    void enableControls(bool enable) { controlsEnabled = enable; }
    bool areControlsEnabled() const { return controlsEnabled; }
    
    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    void rotate(float yaw, float pitch);
    
    virtual void drawInspector() override;
    
    glm::vec4 getViewport() const { return viewport; }
    void setViewport(const glm::vec4& vp) { viewport = vp; }
    void setViewport(float x, float y, float width, float height) { 
        viewport = glm::vec4(x, y, width, height); 
    }
    
#ifdef EDITOR_BUILD
    bool getShowGizmo() const { return showGizmo; }
    void setShowGizmo(bool show) { showGizmo = show; }
    
    bool getShowFrustum() const { return showFrustum; }
    void setShowFrustum(bool show) { showFrustum = show; }
    
    std::shared_ptr<Mesh> getGizmoMesh() const { return gizmoMesh; }
    std::shared_ptr<Material> getGizmoMaterial() const { return gizmoMaterial; }
    std::shared_ptr<Mesh> getFrustumMesh() const { return frustumMesh; }
    std::shared_ptr<Material> getFrustumMaterial() const { return frustumMaterial; }
#endif
    
private:
    ProjectionType projectionType;
    
    float fov;
    float aspectRatio;
    float nearPlane;
    float farPlane;
    
    float orthographicSize;
    
    glm::vec4 viewport;
    
    bool isActiveCamera;
    bool controlsEnabled;
    
    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;
    
    mutable glm::mat4 projectionMatrix;
    mutable bool projectionDirty;
    
    void updateProjection();
    
    void handleKeyboardInput(float deltaTime);
    void handleMouseInput();
    void updateRotation();
    
#ifdef EDITOR_BUILD
    bool showGizmo;
    bool showFrustum;
    std::shared_ptr<Mesh> gizmoMesh;
    std::shared_ptr<Material> gizmoMaterial;
    std::shared_ptr<Mesh> frustumMesh;
    std::shared_ptr<Material> frustumMaterial;
    
    void createGizmo();
    void updateGizmo();
    void createFrustumMesh();
    void updateFrustumMesh();
    void calculateFrustumCorners(std::vector<glm::vec3>& corners) const;
#endif
};

} // namespace GameEngine

#endif // CAMERA_COMPONENT_H
