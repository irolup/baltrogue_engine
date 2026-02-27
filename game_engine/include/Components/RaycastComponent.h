#ifndef RAYCAST_COMPONENT_H
#define RAYCAST_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <string>

namespace GameEngine {

class Material;

class RaycastComponent : public Component {
public:
    RaycastComponent();
    virtual ~RaycastComponent() = default;

    COMPONENT_TYPE(RaycastComponent)

    virtual void start() override {}
    virtual void update(float deltaTime) override {}
    virtual void drawInspector() override;

    void setFrom(const glm::vec3& from);
    glm::vec3 getFrom() const { return from_; }
    void setTo(const glm::vec3& to);
    glm::vec3 getTo() const { return to_; }
    void setCollisionMask(int mask);
    int getCollisionMask() const { return collisionMask_; }
    void setShowDebugLine(bool show);
    bool getShowDebugLine() const { return showDebugLine_; }

    glm::vec3 getWorldFrom() const;
    glm::vec3 getWorldTo() const;

    bool performRaycast(bool& hit, std::string& hitNodeName, glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance) const;

#if defined(EDITOR_BUILD) || defined(LINUX_BUILD)
    void renderDebugLine(Material& debugMaterial, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
#endif

private:
    glm::vec3 from_;
    glm::vec3 to_;
    int collisionMask_;
    bool showDebugLine_;
};

} // namespace GameEngine

#endif // RAYCAST_COMPONENT_H
