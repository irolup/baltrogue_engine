#ifndef JOINT_COMPONENT_H
#define JOINT_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <string>

class btTypedConstraint;

namespace GameEngine {

enum class JointType {
    FIXED 
    // POINT, HINGE... can be added later
};

class JointComponent : public Component {
public:
    JointComponent();
    virtual ~JointComponent();
    
    COMPONENT_TYPE(JointComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void destroy() override;
    
    void setBodyA(const std::string& nodeName);
    void setBodyB(const std::string& nodeName);
    const std::string& getBodyA() const { return bodyAName; }
    const std::string& getBodyB() const { return bodyBName; }
    
    void setJointType(JointType type);
    JointType getJointType() const { return jointType; }
    
    // Pivot in local space
    void setPivotA(const glm::vec3& pivot);
    glm::vec3 getPivotA() const { return pivotA; }
    
    // Pivot in local space
    void setPivotB(const glm::vec3& pivot);
    glm::vec3 getPivotB() const { return pivotB; }
    
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled; }
    
    btTypedConstraint* getConstraint() const { return constraint; }
    
    virtual void drawInspector() override;
    
private:
    std::string bodyAName;
    std::string bodyBName;
    JointType jointType;
    glm::vec3 pivotA;
    glm::vec3 pivotB;
    bool enabled;
    
    btTypedConstraint* constraint;
    bool constraintCreatePending;
    
    void createConstraint();
    void destroyConstraint();
};

} // namespace GameEngine

#endif // JOINT_COMPONENT_H
