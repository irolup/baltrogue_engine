#include "Rendering/ShadowMap.h"

#include "Components/LightComponent.h"
#include "Rendering/LightingManager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace GameEngine {

namespace {

float shadowNearPlane(float range) {
    return glm::clamp(range * 0.05f, 0.05f, 1.0f);
}

// Vulkan clips to 0 <= z <= w while GL and VitaGL clip to -w <= z <= w. glm
// builds GL style projections, so on Vulkan the depth range is remapped or half
// of every light frustum would be clipped away. The lit shaders mirror this:
// the GL/Cg ones map the sampled z back to [0,1], the SPIR-V one does not.
// Only the matrix handed to the GPU is remapped. Frustum culling keeps using
// the GL style matrix, because the plane extraction in Frustum assumes the
// symmetric -w..w depth range

glm::mat4 toClipSpace(const glm::mat4& viewProjection) {
#ifdef ENABLE_VULKAN
    glm::mat4 depthRemap(1.0f);
    depthRemap[2][2] = 0.5f;
    depthRemap[3][2] = 0.5f;
    return depthRemap * viewProjection;
#else
    return viewProjection;
#endif
}

glm::vec3 pickUpVector(const glm::vec3& direction) {
    return (std::fabs(direction.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback) {
    const float lengthSquared = glm::dot(v, v);
    if (lengthSquared < 1e-8f) {
        return fallback;
    }
    return v / std::sqrt(lengthSquared);
}

void writeSettingsBlock(std::ofstream& file, const char* platform, const ShadowSettings& settings) {
    file << platform << ":enabled," << (settings.enabled ? 1 : 0) << "\n";
    file << platform << ":tileSize," << settings.tileSize << "\n";
    file << platform << ":softShadows," << (settings.softShadows ? 1 : 0) << "\n";
    file << platform << ":directionalExtent," << settings.directionalExtent << "\n";
    file << platform << ":directionalDepth," << settings.directionalDepth << "\n";
}

void applySetting(ShadowSettings& settings, const std::string& key, const std::string& value) {
    if (key == "enabled") {
        settings.enabled = (std::atoi(value.c_str()) != 0);
    } else if (key == "tileSize") {
        // Clamped: the atlas is tileSize * 4 by tileSize * 2, and a hand edited
        // file must not be able to ask for a texture no GPU here can allocate.
        settings.tileSize = glm::clamp(std::atoi(value.c_str()), 64, 1024);
    } else if (key == "softShadows") {
        settings.softShadows = (std::atoi(value.c_str()) != 0);
    } else if (key == "directionalExtent") {
        settings.directionalExtent = static_cast<float>(std::atof(value.c_str()));
    } else if (key == "directionalDepth") {
        settings.directionalDepth = static_cast<float>(std::atof(value.c_str()));
    }
}

}

ShadowManager::ShadowManager()
    : pcSettings(getDefaultSettings("pc"))
    , vitaSettings(getDefaultSettings("vita"))
{
    views.reserve(kMaxShadowViews);
    viewMatrices.reserve(kMaxShadowViews);
}

ShadowSettings ShadowManager::getDefaultSettings(const std::string& platform) {
    ShadowSettings defaults;
    if (platform != "vita") {
        defaults.tileSize = 512;
        defaults.softShadows = true;
    }
    return defaults;
}

ShadowSettings& ShadowManager::getSettings() {
#ifdef VITA_BUILD
    return vitaSettings;
#else
    return pcSettings;
#endif
}

const ShadowSettings& ShadowManager::getSettings() const {
#ifdef VITA_BUILD
    return vitaSettings;
#else
    return pcSettings;
#endif
}

ShadowSettings& ShadowManager::getSettingsForPlatform(const std::string& platform) {
    return (platform == "vita") ? vitaSettings : pcSettings;
}

bool ShadowManager::loadSettings(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const size_t comma = line.find(',', colon);
        if (comma == std::string::npos) {
            continue;
        }

        const std::string platform = line.substr(0, colon);
        if (platform != "pc" && platform != "vita") {
            continue;
        }

        applySetting(getSettingsForPlatform(platform),
                     line.substr(colon + 1, comma - colon - 1),
                     line.substr(comma + 1));
    }

    return true;
}

bool ShadowManager::saveSettings(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ShadowManager: could not write " << filepath << std::endl;
        return false;
    }

    file << "# Shadow Settings\n";
    file << "# Format: platform:key,value\n";
    file << "# Written by the editor. Each build reads its own platform's block:\n";
    file << "#   pc   - the desktop game and the editor viewport\n";
    file << "#   vita - the Vita build (packed into the VPK)\n\n";

    writeSettingsBlock(file, "pc", pcSettings);
    file << "\n";
    writeSettingsBlock(file, "vita", vitaSettings);

    return true;
}

ShadowManager& ShadowManager::getInstance() {
    static ShadowManager instance;
    return instance;
}

void ShadowManager::clear() {
    views.clear();
    viewMatrices.clear();
}

glm::ivec4 ShadowManager::getTileViewport(uint32_t tile) const {
    const int tileSize = getSettings().tileSize;
    const uint32_t col = tile % kShadowAtlasCols;
    const uint32_t row = tile / kShadowAtlasCols;
    return glm::ivec4(static_cast<int>(col) * tileSize,
                      static_cast<int>(row) * tileSize,
                      tileSize,
                      tileSize);
}

glm::vec4 ShadowManager::getShaderParams() const {
    const float width = static_cast<float>(getAtlasWidth());
    const float height = static_cast<float>(getAtlasHeight());
    return glm::vec4(width > 0.0f ? 1.0f / width : 0.0f,
                     height > 0.0f ? 1.0f / height : 0.0f,
                     getSettings().softShadows ? 1.0f : 0.0f,
                     0.0f);
}

void ShadowManager::addView(const glm::mat4& viewProjection) {
    ShadowView view;
    view.viewProjection = toClipSpace(viewProjection);
    view.frustum.update(viewProjection);
    view.tile = static_cast<uint32_t>(views.size());

    views.push_back(view);
    viewMatrices.push_back(view.viewProjection);
}

void ShadowManager::addDirectionalViews(LightComponent& light, const glm::vec3& cameraPosition, const glm::vec3& cameraForward) {
    const glm::vec3 lightDirection = safeNormalize(light.getDirection(), glm::vec3(0.0f, -1.0f, 0.0f));
    const glm::vec3 up = pickUpVector(lightDirection);
    const ShadowSettings& settings = getSettings();
    const float extent = std::max(settings.directionalExtent, 1.0f);
    const float depth = std::max(settings.directionalDepth, extent);

    // Follow the camera so the limited ortho box covers what the player sees
    glm::vec3 center = cameraPosition + cameraForward * extent * 0.5f;

    // Snap the box to whole texels, otherwise the shadow edges crawl as the
    // camera moves
    const glm::mat4 lightRotation = glm::lookAt(glm::vec3(0.0f), lightDirection, up);
    glm::vec3 centerInLightSpace = glm::vec3(lightRotation * glm::vec4(center, 1.0f));
    const float texelSize = (2.0f * extent) / static_cast<float>(std::max(settings.tileSize, 1));
    centerInLightSpace.x = std::floor(centerInLightSpace.x / texelSize) * texelSize;
    centerInLightSpace.y = std::floor(centerInLightSpace.y / texelSize) * texelSize;
    center = glm::vec3(glm::inverse(lightRotation) * glm::vec4(centerInLightSpace, 1.0f));

    const glm::mat4 view = glm::lookAt(center - lightDirection * depth, center, up);
    const glm::mat4 projection = glm::ortho(-extent, extent, -extent, extent, 0.0f, 2.0f * depth);

    addView(projection * view);
}

void ShadowManager::addSpotViews(LightComponent& light) {
    const glm::vec3 position = light.getPosition();
    const glm::vec3 direction = safeNormalize(light.getDirection(), glm::vec3(0.0f, -1.0f, 0.0f));
    const glm::vec3 up = pickUpVector(direction);

    // A little wider than the cone so the penumbra at the edge is not cut off
    const float fieldOfView = glm::clamp(2.0f * light.getOuterCutOff() * 1.1f, glm::radians(5.0f), glm::radians(175.0f));
    const float nearPlane = shadowNearPlane(light.getRange());
    const float farPlane = std::max(light.getRange(), nearPlane * 2.0f);

    const glm::mat4 view = glm::lookAt(position, position + direction, up);
    const glm::mat4 projection = glm::perspective(fieldOfView, 1.0f, nearPlane, farPlane);

    addView(projection * view);
}

void ShadowManager::addPointViews(LightComponent& light) {
    // Face order must match cubeFaceIndex() in the lit shaders: +X -X +Y -Y +Z -Z
    static const glm::vec3 faceDirections[kShadowViewsPerPointLight] = {
        glm::vec3( 1.0f,  0.0f,  0.0f),
        glm::vec3(-1.0f,  0.0f,  0.0f),
        glm::vec3( 0.0f,  1.0f,  0.0f),
        glm::vec3( 0.0f, -1.0f,  0.0f),
        glm::vec3( 0.0f,  0.0f,  1.0f),
        glm::vec3( 0.0f,  0.0f, -1.0f),
    };

    const glm::vec3 position = light.getPosition();
    const float nearPlane = shadowNearPlane(light.getRange());
    const float farPlane = std::max(light.getRange(), nearPlane * 2.0f);
    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

    for (uint32_t face = 0; face < kShadowViewsPerPointLight; ++face) {
        const glm::vec3& direction = faceDirections[face];
        const glm::mat4 view = glm::lookAt(position, position + direction, pickUpVector(direction));
        addView(projection * view);
    }
}

void ShadowManager::update(const glm::vec3& cameraPosition, const glm::vec3& cameraForward) {
    clear();

    auto& lightingManager = LightingManager::getInstance();
    const std::vector<LightComponent*>& lights = lightingManager.getLights();

    const size_t uploadedLightCount = lightingManager.getActiveLightCount();
    for (size_t i = 0; i < lights.size(); ++i) {
        if (lights[i]) {
            lights[i]->setShadowViewIndex(-1);
        }
    }

    if (!getSettings().enabled) {
        return;
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (size_t i = 0; i < uploadedLightCount && i < lights.size(); ++i) {
            LightComponent* light = lights[i];
            if (!light || !light->isEnabled() || !light->getCastShadows()) {
                continue;
            }

            const LightType type = light->getType();
            uint32_t requiredTiles = 1;
            if (pass == 0) {
                if (type != LightType::DIRECTIONAL) continue;
            } else if (pass == 1) {
                if (type != LightType::SPOT) continue;
            } else {
                if (type != LightType::POINT) continue;
                requiredTiles = kShadowViewsPerPointLight;
            }

            if (requiredTiles > remainingTiles()) {
                continue;
            }

            light->setShadowViewIndex(static_cast<int>(views.size()));

            if (type == LightType::DIRECTIONAL) {
                addDirectionalViews(*light, cameraPosition, cameraForward);
            } else if (type == LightType::SPOT) {
                addSpotViews(*light);
            } else {
                addPointViews(*light);
            }
        }
    }
}

}
