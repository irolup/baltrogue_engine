#ifndef COMPONENT_H
#define COMPONENT_H

#include <string>
#include <typeinfo>

namespace GameEngine {

class SceneNode;
class Renderer;

class Component {
public:
    Component() : owner(nullptr), enabled(true) {}
    virtual ~Component() = default;
    
    // Component lifecycle
    virtual void start() {} // Called when component is first added
    virtual void update(float deltaTime) {} // Called every frame
    virtual void render(Renderer& renderer) {} // Called during rendering
    virtual void destroy() {} // Called before component is destroyed
    
    // Properties
    bool isEnabled() const { return enabled; }
    void setEnabled(bool state) { enabled = state; }
    
    // Owner node
    SceneNode* getOwner() const { return owner; }
    void setOwner(SceneNode* node) { owner = node; }
    
    // Component type identification
    virtual std::string getTypeName() const = 0;
    
    // Editor support
    virtual void drawInspector() {} // For editor property panels
    
protected:
    SceneNode* owner;
    bool enabled;
};

// Macro to help implement getTypeName for derived components
#define COMPONENT_TYPE(ClassName) \
    virtual std::string getTypeName() const override { return #ClassName; }

} // namespace GameEngine

#endif // COMPONENT_H
