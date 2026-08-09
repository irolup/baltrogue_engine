#ifndef LIGHTING_MANAGER_H
#define LIGHTING_MANAGER_H

#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>
#include "Components/LightComponent.h"

namespace GameEngine {

class LightingManager {
public:
    static LightingManager& getInstance();
    
    void addLight(LightComponent* light);
    void removeLight(LightComponent* light);
    void clearLights();
    
    const std::vector<LightComponent*>& getLights() const { return lights; }
    
    const std::vector<LightComponent::LightData>& getLightDataArray() const;
    
    static constexpr size_t MAX_LIGHTS = 16;
    size_t getActiveLightCount() const { return std::min(lights.size(), MAX_LIGHTS); }
    size_t getLightCount() const { return lights.size(); }

    void update();

    void beginPass() { ++passStamp; }
    uint32_t getPassStamp() const { return passStamp; }

    bool isShadowMapBound() const { return shadowMapBound; }
    void setShadowMapBound(bool bound) { shadowMapBound = bound; }

private:
    LightingManager() = default;
    ~LightingManager() = default;
    LightingManager(const LightingManager&) = delete;
    LightingManager& operator=(const LightingManager&) = delete;
    
    std::vector<LightComponent*> lights;
    mutable std::vector<LightComponent::LightData> lightDataScratch;
    uint32_t passStamp = 1;
    bool shadowMapBound = false;
};

} // namespace GameEngine

#endif // LIGHTING_MANAGER_H
