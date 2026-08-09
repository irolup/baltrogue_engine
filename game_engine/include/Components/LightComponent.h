#ifndef LIGHT_COMPONENT_H
#define LIGHT_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <memory>

namespace GameEngine {

class Mesh;
class Material;

enum class LightType {
    DIRECTIONAL = 0,
    POINT = 1,
    SPOT = 2
};

class LightComponent : public Component {
public:
    LightComponent();
    virtual ~LightComponent();
    
    void start() override;
    void update(float deltaTime) override;
    void render(IRenderer& renderer) override;
    void destroy() override;
    void suspend() override;
    void resume() override;
    
    LightType getType() const { return type; }
    void setType(LightType newType);
    
    glm::vec3 getColor() const { return color; }
    void setColor(const glm::vec3& newColor) { color = newColor; }
    
    glm::vec3 getPosition() const;
    void setPosition(const glm::vec3& newPosition);
    
    glm::vec3 getDirection() const;
    void setDirection(const glm::vec3& newDirection);
    
    float getIntensity() const { return intensity; }
    void setIntensity(float newIntensity) { intensity = newIntensity; }
    
    float getCutOff() const { return cutOff; }
    void setCutOff(float newCutOff) { cutOff = newCutOff; }
    
    float getOuterCutOff() const { return outerCutOff; }
    void setOuterCutOff(float newOuterCutOff) { outerCutOff = newOuterCutOff; }
    
    float getRange() const { return range; }
    void setRange(float newRange) { range = newRange; }
    
    float getConstant() const { return constant; }
    void setConstant(float newConstant) { constant = newConstant; }
    
    float getLinear() const { return linear; }
    void setLinear(float newLinear) { linear = newLinear; }
    
    float getQuadratic() const { return quadratic; }
    void setQuadratic(float newQuadratic) { quadratic = newQuadratic; }
    
    bool getCastShadows() const { return castShadows; }
    void setCastShadows(bool cast) { castShadows = cast; }

    float getShadowStrength() const { return shadowStrength; }
    void setShadowStrength(float strength) { shadowStrength = strength; }

    float getShadowBias() const { return shadowBias; }
    void setShadowBias(float bias) { shadowBias = bias; }

    int getShadowViewIndex() const { return shadowViewIndex; }
    void setShadowViewIndex(int index) { shadowViewIndex = index; }

    bool getShowGizmo() const { return showGizmo; }
    void setShowGizmo(bool show);
    
    std::shared_ptr<Mesh> getGizmoMesh() const { return gizmoMesh; }
    std::shared_ptr<Material> getGizmoMaterial() const { return gizmoMaterial; }
    
    COMPONENT_TYPE(LightComponent)
    
    void drawInspector() override;
    
    struct LightData {
        glm::vec4 position; // xyz = world position, w = light type
        glm::vec4 direction; // xyz = world direction, w = intensity
        glm::vec4 color; // rgb = colour, a = range
        glm::vec4 params;  // cutOff, outerCutOff, constant, linear
        glm::vec4 attenuation; // quadratic, shadow tile index (-1 = none), strength, bias
    };
    
    LightData getLightData() const;
    
private:
    LightType type;
    
    glm::vec3 color;
    float intensity;
    
    glm::vec3 localPosition;
    glm::vec3 localDirection;
    
    float cutOff;
    float outerCutOff;
    
    float range;
    float constant;
    float linear;
    float quadratic;

    bool castShadows;
    float shadowStrength;
    float shadowBias;
    int shadowViewIndex;

    bool showGizmo;
    std::shared_ptr<Mesh> gizmoMesh;
    std::shared_ptr<Material> gizmoMaterial;
    
    void updateGizmo();
    void createGizmo();
    void updateLightData();
};

} // namespace GameEngine

#endif // LIGHT_COMPONENT_H
