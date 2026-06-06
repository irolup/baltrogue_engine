#include "Components/BeamRenderer.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureManager.h"
#include "Scene/SceneNode.h"

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

BeamRenderer::BeamRenderer()
    : beamMesh(Mesh::createBeam())
    , material(nullptr)
    , beamStart(0.0f)
    , beamEnd(0.0f, 0.0f, -1.0f)
    , beamHalfWidth(0.04f)
{
#ifdef EDITOR_BUILD
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
#endif
}

BeamRenderer::~BeamRenderer() {
}

void BeamRenderer::render(IRenderer& renderer) {
    if (!beamMesh || !material || !owner) {
        return;
    }
    RenderCommand command;
    command.mesh = beamMesh;
    command.material = material;
    command.modelMatrix = glm::mat4(1.0f);
    command.normalMatrix = glm::mat3(1.0f);
    command.isBeam = true;
    command.beamStart = beamStart;
    command.beamEnd = beamEnd;
    command.beamHalfWidth = beamHalfWidth;
    command.disableCulling = true;
    renderer.submitRenderCommand(command);
}

void BeamRenderer::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Beam Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Material: %s", material ? "Loaded" : "None");
        ImGui::Text("Start: (%.2f, %.2f, %.2f)", beamStart.x, beamStart.y, beamStart.z);
        ImGui::Text("End: (%.2f, %.2f, %.2f)", beamEnd.x, beamEnd.y, beamEnd.z);
        float w = beamHalfWidth * 2.0f;
        if (ImGui::DragFloat("Width", &w, 0.001f, 0.001f, 1.0f)) {
            beamHalfWidth = w * 0.5f;
        }
        if (material) {
            ImGui::Separator();
            material->drawInspector();
        }
    }
#endif
}

} // namespace GameEngine
