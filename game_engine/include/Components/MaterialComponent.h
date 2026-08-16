#ifndef MATERIAL_COMPONENT_H
#define MATERIAL_COMPONENT_H

#include "Components/Component.h"
#include "Rendering/Material.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace GameEngine {

// Runtime override layer for the materials a renderer already has. Attach it next
// to a ModelRenderer to retune the factors a glTF material baked in
// while keeping the base color / normal / ARM
// maps that came with the model, or next to a MeshRenderer for the same on a
// primitive. Textures and the shader can be swapped too, but only when asked for.
class MaterialComponent : public Component {
public:
    MaterialComponent();
    virtual ~MaterialComponent();

    COMPONENT_TYPE(MaterialComponent)

    const MaterialOverride& getOverrides() const { return overrides; }
    void setOverrides(const MaterialOverride& newOverrides);

    MaterialOverride& editOverrides();

    void clearOverrides();

    std::shared_ptr<Material> resolveMaterial(const std::shared_ptr<Material>& baseMaterial);

    void clearResolvedMaterials();

    virtual void drawInspector() override;

private:
    struct ResolvedMaterial {
        std::shared_ptr<Material> base;
        std::shared_ptr<Material> copy;
        uint32_t appliedRevision = 0;
    };

    MaterialOverride overrides;
    // Starts at 1 so a freshly cached copy (revision 0) always gets one apply pass
    uint32_t revision = 1;
    std::unordered_map<const Material*, ResolvedMaterial> resolvedMaterials;

    void drawFactorOverrides();
    void drawTextureOverrides();
    void drawShaderOverrides();
    void drawTexturePicker(const char* label, std::string& texturePath);
};

}

#endif