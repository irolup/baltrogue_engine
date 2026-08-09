#ifndef COMPONENT_H
#define COMPONENT_H

#include <string>
#include <typeinfo>
namespace GameEngine {

class SceneNode;
class IRenderer;
class Renderer;

class Component {
public:
    Component() : owner(nullptr), enabled(true) {}
    virtual ~Component() = default;
    
    // Component lifecycle
    virtual void start() {} // Called when component is first added
    virtual void update(float deltaTime) {} // Every frame
    virtual void fixedUpdate(float deltaTime) {} // Fixed rate (60 Hz)
    virtual void lateUpdate(float deltaTime) {} // Every frame after physics
    virtual void render(IRenderer& renderer) {}
    virtual void destroy() {}
    virtual void prepareForRestart() {}
    virtual void suspend() {}
    virtual void resume() {}
    
    // Properties
    bool isEnabled() const { return enabled; }
    void setEnabled(bool state) { enabled = state; }
    
    // Owner node
    SceneNode* getOwner() const { return owner; }
    void setOwner(SceneNode* node) { owner = node; }
    
    // Component type identification
    virtual const std::string& getTypeName() const = 0;
    
    // Editor support
    virtual void drawInspector() {} // For editor property panels
    
protected:
    SceneNode* owner;
    bool enabled;
};

// Macro to help implement getTypeName for derived components
#define COMPONENT_TYPE(ClassName) \
    static const std::string& StaticTypeName() { \
        static const std::string typeName = #ClassName; \
        return typeName; \
    } \
    virtual const std::string& getTypeName() const override { \
        return StaticTypeName(); \
    }

} // namespace GameEngine

#endif // COMPONENT_H
