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

const std::vector<LightComponent::LightData>& LightingManager::getLightDataArray() const {
    lightDataScratch.clear();
    lightDataScratch.reserve(MAX_LIGHTS);

    size_t count = std::min(lights.size(), MAX_LIGHTS);
    for (size_t i = 0; i < count; ++i) {
        if (lights[i] && lights[i]->isEnabled()) {
            lightDataScratch.push_back(lights[i]->getLightData());
        }
    }

    while (lightDataScratch.size() < MAX_LIGHTS) {
        LightComponent::LightData emptyLight;
        emptyLight.position = glm::vec4(0.0f);
        emptyLight.direction = glm::vec4(0.0f);
        emptyLight.color = glm::vec4(0.0f);
        emptyLight.params = glm::vec4(0.0f);
        emptyLight.attenuation = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        lightDataScratch.push_back(emptyLight);
    }

    return lightDataScratch;
}

void LightingManager::update() {
    lights.erase(
        std::remove(lights.begin(), lights.end(), nullptr),
        lights.end()
    );
}

} // namespace GameEngine
