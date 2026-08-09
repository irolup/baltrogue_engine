#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

#include "Rendering/Frustum.h"

namespace GameEngine {

class LightComponent;

// Every shadow map is a tile of one atlas texture. Directional and spot lights
// take a single tile, point lights take six , so this constant also caps how 
// many shadow casters a scene can afford. It is mirrored
// by u_ShadowMatrices in the lit shaders of all three backends, keep them in
// sync when changing it.
static const uint32_t kMaxShadowViews = 8;
static const uint32_t kShadowAtlasCols = 4;
static const uint32_t kShadowAtlasRows = 2;
static const uint32_t kShadowViewsPerPointLight = 6;

// Texture unit the shadow atlas occupies in the GL and VitaGL backends. Units
// 0-3 are the lit shader's diffuse/normal/ARM/environment slots
static const int kShadowMapTextureUnit = 4;

static const char* const kShadowSettingsPath = "config/shadow_settings.txt";

struct ShadowView {
    glm::mat4 viewProjection;
    Frustum frustum;
    uint32_t tile;

    ShadowView() : viewProjection(1.0f), tile(0) {}
};

struct ShadowSettings {
    bool enabled = true;
    int tileSize = 256;
    bool softShadows = false;
    float directionalExtent = 25.0f;
    float directionalDepth = 60.0f;
};

class ShadowManager {
public:
    static ShadowManager& getInstance();

    void update(const glm::vec3& cameraPosition, const glm::vec3& cameraForward);

    void clear();

    bool hasShadows() const { return !views.empty(); }
    const std::vector<ShadowView>& getViews() const { return views; }
    size_t getViewCount() const { return views.size(); }

    const std::vector<glm::mat4>& getViewMatrices() const { return viewMatrices; }

    ShadowSettings& getSettings();
    const ShadowSettings& getSettings() const;

    ShadowSettings& getSettingsForPlatform(const std::string& platform);

    static ShadowSettings getDefaultSettings(const std::string& platform);

    bool loadSettings(const std::string& filepath = kShadowSettingsPath);
    bool saveSettings(const std::string& filepath = kShadowSettingsPath) const;

    int getAtlasWidth() const { return getSettings().tileSize * static_cast<int>(kShadowAtlasCols); }
    int getAtlasHeight() const { return getSettings().tileSize * static_cast<int>(kShadowAtlasRows); }

    glm::ivec4 getTileViewport(uint32_t tile) const;

    glm::vec4 getShaderParams() const;

private:
    ShadowManager();
    ~ShadowManager() = default;
    ShadowManager(const ShadowManager&) = delete;
    ShadowManager& operator=(const ShadowManager&) = delete;

    uint32_t remainingTiles() const { return kMaxShadowViews - static_cast<uint32_t>(views.size()); }

    void addView(const glm::mat4& viewProjection);
    void addDirectionalViews(LightComponent& light, const glm::vec3& cameraPosition, const glm::vec3& cameraForward);
    void addSpotViews(LightComponent& light);
    void addPointViews(LightComponent& light);

    ShadowSettings pcSettings;
    ShadowSettings vitaSettings;
    std::vector<ShadowView> views;
    std::vector<glm::mat4> viewMatrices;
};

}

#endif
