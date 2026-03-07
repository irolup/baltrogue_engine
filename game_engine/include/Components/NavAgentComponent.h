#ifndef NAV_AGENT_COMPONENT_H
#define NAV_AGENT_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <string>

namespace GameEngine {

class PhysicsComponent;
class NavVolumeComponent;
class NavGrid;

class NavAgentComponent : public Component {
public:
    NavAgentComponent();
    virtual ~NavAgentComponent() = default;

    COMPONENT_TYPE(NavAgentComponent)

    virtual void start() override {}
    virtual void update(float deltaTime) override;

    void setSpeed(float speed) { speed_ = speed; }
    float getSpeed() const { return speed_; }
    void setTurnSpeed(float turnSpeed) { turnSpeed_ = turnSpeed; }
    float getTurnSpeed() const { return turnSpeed_; }

    void setDestination(const glm::vec3& worldPos);
    void setFollowPath(const std::vector<glm::vec3>& path);
    void clearDestination();

    bool hasPath() const { return pathIndex_ < (int)path_.size(); }
    bool isMoving() const { return hasPath() && path_.size() > 0; }
    int getPathSize() const { return (int)path_.size(); }
    int getPathIndex() const { return pathIndex_; }
    bool consumePathFinishedFlag();

    void setAssignedVolumeNodeName(const std::string& name) { assignedVolumeNodeName_ = name; }
    const std::string& getAssignedVolumeNodeName() const { return assignedVolumeNodeName_; }

    glm::vec3 getPosition() const;
    glm::vec3 getVelocity() const { return velocity_; }

    virtual void drawInspector() override;

private:
    NavGrid* getGridForAgent() const;

    PhysicsComponent* findChildPhysicsComponent() const;
    PhysicsComponent* findSiblingPhysicsComponent() const;

    float speed_;
    std::vector<glm::vec3> path_;
    int pathIndex_;
    glm::vec3 velocity_;
    bool pathFinishedThisFrame_;
    bool pathJustSet_;
    float turnSpeed_;
    std::string assignedVolumeNodeName_;
};

} // namespace GameEngine

#endif // NAV_AGENT_COMPONENT_H
