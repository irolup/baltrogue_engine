#include "Components/MaterialComponent.h"
#include "Rendering/TextureManager.h"

#ifdef EDITOR_BUILD
    #include "imgui.h"
    #include "Editor/ProjectAssets.h"
#endif

namespace GameEngine {

MaterialComponent::MaterialComponent() {
}

MaterialComponent::~MaterialComponent() {
}

void MaterialComponent::setOverrides(const MaterialOverride& newOverrides) {
    overrides = newOverrides;
    ++revision;
}

MaterialOverride& MaterialComponent::editOverrides() {
    ++revision;
    return overrides;
}

void MaterialComponent::clearOverrides() {
    overrides = MaterialOverride();
    ++revision;
}

void MaterialComponent::clearResolvedMaterials() {
    resolvedMaterials.clear();
}

std::shared_ptr<Material> MaterialComponent::resolveMaterial(const std::shared_ptr<Material>& baseMaterial) {
    if (!baseMaterial || overrides.isEmpty()) {
        return baseMaterial;
    }

    ResolvedMaterial& resolved = resolvedMaterials[baseMaterial.get()];
    if (!resolved.copy) {
        resolved.base = baseMaterial;
        resolved.copy = baseMaterial->clone();
    }

    if (resolved.appliedRevision != revision) {
        *resolved.copy = *resolved.base;
        resolved.copy->applyOverride(overrides);
        resolved.appliedRevision = revision;
    }

    return resolved.copy;
}

void MaterialComponent::drawTexturePicker(const char* label, std::string& texturePath) {
#ifdef EDITOR_BUILD
    auto& textureManager = TextureManager::getInstance();
    const std::string preview = texturePath.empty() ? "From model" : texturePath;

    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("From model", texturePath.empty())) {
            texturePath.clear();
            ++revision;
        }
        for (const auto& availablePath : textureManager.getAvailableTextures()) {
            if (ImGui::Selectable(availablePath.c_str(), availablePath == texturePath)) {
                texturePath = availablePath;
                ++revision;
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginDragDropTarget()) {
        const std::string dropped = ProjectAssets::acceptDrop(ProjectAssets::Kind::Texture);
        if (!dropped.empty()) {
            texturePath = dropped;
            ++revision;
        }
        ImGui::EndDragDropTarget();
    }
#else
    (void)label;
    (void)texturePath;
#endif
}

void MaterialComponent::drawFactorOverrides() {
#ifdef EDITOR_BUILD
    if (ImGui::Checkbox("Base Color", &overrides.overrideBaseColor)) ++revision;
    if (overrides.overrideBaseColor) {
        ImGui::SameLine();
        if (ImGui::ColorEdit3("##BaseColor", &overrides.baseColor.x)) ++revision;
    }

    if (ImGui::Checkbox("Metallic", &overrides.overrideMetallic)) ++revision;
    if (overrides.overrideMetallic) {
        ImGui::SameLine();
        if (ImGui::SliderFloat("##Metallic", &overrides.metallic, 0.0f, 1.0f)) ++revision;
    }

    if (ImGui::Checkbox("Roughness", &overrides.overrideRoughness)) ++revision;
    if (overrides.overrideRoughness) {
        ImGui::SameLine();
        if (ImGui::SliderFloat("##Roughness", &overrides.roughness, 0.0f, 1.0f)) ++revision;
    }

    if (ImGui::Checkbox("Reflection Strength", &overrides.overrideReflectionStrength)) ++revision;
    if (overrides.overrideReflectionStrength) {
        ImGui::SameLine();
        if (ImGui::SliderFloat("##ReflectionStrength", &overrides.reflectionStrength, 0.0f, 1.0f)) ++revision;
    }

    if (ImGui::Checkbox("Opacity", &overrides.overrideOpacity)) ++revision;
    if (overrides.overrideOpacity) {
        ImGui::SameLine();
        if (ImGui::SliderFloat("##Opacity", &overrides.opacity, 0.0f, 1.0f)) ++revision;
    }

    if (ImGui::Checkbox("Alpha Cutoff", &overrides.overrideAlphaCutoff)) ++revision;
    if (overrides.overrideAlphaCutoff) {
        ImGui::SameLine();
        if (ImGui::SliderFloat("##AlphaCutoff", &overrides.alphaCutoff, 0.0f, 1.0f)) ++revision;
        ImGui::TextWrapped("0 disables the alpha test. Above 0 the fragment is discarded below that alpha.");
    }

    if (ImGui::Checkbox("Blend Mode", &overrides.overrideBlendMode)) ++revision;
    if (overrides.overrideBlendMode) {
        ImGui::SameLine();
        const char* blendModeNames[] = { "Opaque", "Alpha", "Additive" };
        int currentBlend = static_cast<int>(overrides.blendMode);
        if (ImGui::Combo("##BlendMode", &currentBlend, blendModeNames, 3)) {
            overrides.blendMode = static_cast<BlendMode>(currentBlend);
            ++revision;
        }
    }

    if (ImGui::Checkbox("Double Sided", &overrides.overrideDoubleSided)) ++revision;
    if (overrides.overrideDoubleSided) {
        ImGui::SameLine();
        if (ImGui::Checkbox("##DoubleSided", &overrides.doubleSided)) ++revision;
    }

    if (ImGui::Checkbox("UV Transform", &overrides.overrideUVTransform)) ++revision;
    if (overrides.overrideUVTransform) {
        if (ImGui::DragFloat2("UV Scale", &overrides.uvScale.x, 0.01f)) ++revision;
        if (ImGui::DragFloat2("UV Offset", &overrides.uvOffset.x, 0.01f)) ++revision;
    }
#endif
}

void MaterialComponent::drawTextureOverrides() {
#ifdef EDITOR_BUILD
    ImGui::TextWrapped("Leave a slot on \"From model\" to keep the map the model loaded.");

    if (ImGui::Button("Refresh Texture List")) {
        TextureManager::getInstance().discoverAllTextures("assets/textures");
    }

    drawTexturePicker("Diffuse", overrides.diffuseTexturePath);
    drawTexturePicker("Normal", overrides.normalTexturePath);
    drawTexturePicker("ARM", overrides.armTexturePath);
#endif
}

void MaterialComponent::drawShaderOverrides() {
#ifdef EDITOR_BUILD
    const bool hasLinux = !overrides.shaderVertexPathLinux.empty() && !overrides.shaderFragmentPathLinux.empty();
    const bool hasVita = !overrides.shaderVertexPathVita.empty() && !overrides.shaderFragmentPathVita.empty();
    const bool hasVulkan = !overrides.shaderVertexPathVulkan.empty() && !overrides.shaderFragmentPathVulkan.empty();

    if (!hasLinux && !hasVita && !hasVulkan) {
        ImGui::Text("Shader: from model (default PBR)");
    } else {
        if (hasLinux) {
            ImGui::Text("Linux (GLSL):");
            ImGui::Text("  Vertex: %s", overrides.shaderVertexPathLinux.c_str());
            ImGui::Text("  Fragment: %s", overrides.shaderFragmentPathLinux.c_str());
        }
        if (hasVita) {
            ImGui::Text("Vita (CG):");
            ImGui::Text("  Vertex: %s", overrides.shaderVertexPathVita.c_str());
            ImGui::Text("  Fragment: %s", overrides.shaderFragmentPathVita.c_str());
        }
        if (hasVulkan) {
            ImGui::Text("Vulkan (GLSL/SPIR-V):");
            ImGui::Text("  Vertex: %s", overrides.shaderVertexPathVulkan.c_str());
            ImGui::Text("  Fragment: %s", overrides.shaderFragmentPathVulkan.c_str());
        }
    }

    if (ImGui::Button("Assign Shader from Files")) {
        ImGui::OpenPopup("MaterialComponentShaderPopup");
    }

    std::string vertexPath;
    std::string fragmentPath;
    std::string platform;
    if (Material::drawShaderAssignPopup("MaterialComponentShaderPopup", vertexPath, fragmentPath, platform)) {
        if (platform == "vita") {
            overrides.shaderVertexPathVita = vertexPath;
            overrides.shaderFragmentPathVita = fragmentPath;
        } else if (platform == "vulkan") {
            overrides.shaderVertexPathVulkan = vertexPath;
            overrides.shaderFragmentPathVulkan = fragmentPath;
        } else {
            overrides.shaderVertexPathLinux = vertexPath;
            overrides.shaderFragmentPathLinux = fragmentPath;
        }
        ++revision;
    }

    ImGui::SameLine();
    if (ImGui::Button("Use Model Shader")) {
        overrides.shaderVertexPathLinux.clear();
        overrides.shaderFragmentPathLinux.clear();
        overrides.shaderVertexPathVita.clear();
        overrides.shaderFragmentPathVita.clear();
        overrides.shaderVertexPathVulkan.clear();
        overrides.shaderFragmentPathVulkan.clear();
        ++revision;
    }
#endif
}

void MaterialComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Material Overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Overrides the materials of the MeshRenderer or ModelRenderer on this node. Unchecked values stay as the model loaded them.");

        if (ImGui::Button("Clear All Overrides")) {
            clearOverrides();
        }

        ImGui::Separator();
        ImGui::Text("Factors");
        drawFactorOverrides();

        ImGui::Separator();
        ImGui::Text("Textures");
        drawTextureOverrides();

        ImGui::Separator();
        ImGui::Text("Shader");
        drawShaderOverrides();
    }
#endif
}

}
