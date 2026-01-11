#ifndef LIGHTING_MANAGER_H
#define LIGHTING_MANAGER_H

#include <vector>
#include <memory>
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
    
    std::vector<LightComponent::LightData> getLightDataArray() const;
    
    static constexpr size_t MAX_LIGHTS = 16;
    size_t getActiveLightCount() const { return std::min(lights.size(), MAX_LIGHTS); }
    size_t getLightCount() const { return lights.size(); }
    
    void update();
    
private:
    LightingManager() = default;
    ~LightingManager() = default;
    LightingManager(const LightingManager&) = delete;
    LightingManager& operator=(const LightingManager&) = delete;
    
    std::vector<LightComponent*> lights;
};

} // namespace GameEngine

#endif // LIGHTING_MANAGER_H
