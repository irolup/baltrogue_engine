#include "Components/MeshRenderer.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureManager.h"
#include "Scene/SceneNode.h"
#include <iostream>

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

MeshRenderer::MeshRenderer()
    : mesh(nullptr)
    , material(nullptr)
    , castShadows(true)
    , receiveShadows(true)
{
    // Initialize texture discovery for the editor
#ifdef EDITOR_BUILD
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
#endif
}

MeshRenderer::~MeshRenderer() {
}

void MeshRenderer::render(Renderer& renderer) {
    if (!mesh || !material || !owner) {
        return;
    }
    
    // Get the world transform matrix
    glm::mat4 modelMatrix = owner->getWorldMatrix();
    
    // Create render command
    RenderCommand command;
    command.mesh = mesh;
    command.material = material;
    command.modelMatrix = modelMatrix;
    command.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
    
    // Submit to renderer
    renderer.submitRenderCommand(command);
}

void MeshRenderer::setMesh(std::shared_ptr<Mesh> newMesh) {
    mesh = newMesh;
}

void MeshRenderer::setMaterial(std::shared_ptr<Material> newMaterial) {
    material = newMaterial;
}

void MeshRenderer::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Mesh: %s", mesh ? "Loaded" : "None");
        ImGui::Text("Material: %s", material ? "Loaded" : "None");
        
        ImGui::Checkbox("Cast Shadows", &castShadows);
        ImGui::Checkbox("Receive Shadows", &receiveShadows);
        
        if (mesh) {
            ImGui::Text("Vertices: %zu", mesh->getVertexCount());
            ImGui::Text("Triangles: %zu", mesh->getTriangleCount());
        }
        
        // Show material inspector if material is present
        if (material) {
            ImGui::Separator();
            material->drawInspector();
        }
    }
#endif
}

} // namespace GameEngine
