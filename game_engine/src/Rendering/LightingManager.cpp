#include "Rendering/LightingManager.h"
#include <algorithm>

namespace GameEngine {

LightingManager& LightingManager::getInstance() {
    static LightingManager instance;
    return instance;
}

void LightingManager::addLight(LightComponent* light) {
    if (light && std::find(lights.begin(), lights.end(), light) == lights.end()) {
        lights.push_back(light);
    }
}

void LightingManager::removeLight(LightComponent* light) {
    auto it = std::find(lights.begin(), lights.end(), light);
    if (it != lights.end()) {
        lights.erase(it);
    }
}

void LightingManager::clearLights() {
    lights.clear();
}

std::vector<LightComponent::LightData> LightingManager::getLightDataArray() const {
    std::vector<LightComponent::LightData> lightDataArray;
    lightDataArray.reserve(MAX_LIGHTS);
    
    size_t count = std::min(lights.size(), MAX_LIGHTS);
    for (size_t i = 0; i < count; ++i) {
        if (lights[i] && lights[i]->isEnabled()) {
            lightDataArray.push_back(lights[i]->getLightData());
        }
    }
    
    while (lightDataArray.size() < MAX_LIGHTS) {
        LightComponent::LightData emptyLight;
        emptyLight.position = glm::vec4(0.0f);
        emptyLight.direction = glm::vec4(0.0f);
        emptyLight.color = glm::vec4(0.0f);
        emptyLight.params = glm::vec4(0.0f);
        emptyLight.attenuation = glm::vec4(0.0f);
        lightDataArray.push_back(emptyLight);
    }
    
    return lightDataArray;
}

void LightingManager::update() {
    lights.erase(
        std::remove_if(lights.begin(), lights.end(),
            [](LightComponent* light) {
                return !light || !light->isEnabled();
            }),
        lights.end()
    );
}

} // namespace GameEngine
