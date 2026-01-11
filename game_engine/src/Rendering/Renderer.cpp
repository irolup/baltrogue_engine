#include "Rendering/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/TextComponent.h"
#include "Rendering/Shader.h"
#include "Rendering/LightingManager.h"
#include <algorithm>
#include <iostream>

namespace GameEngine {

Renderer::Renderer()
    : activeCamera(nullptr)
    , viewport(0, 0, 800, 600)
    , clearColor(0.2f, 0.3f, 0.3f)
    , wireframeEnabled(false)
    , depthTestEnabled(true)
    , cullFaceEnabled(false)
{
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize() {
    if (depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    
    if (cullFaceEnabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    
    return true;
}

void Renderer::shutdown() {
}

void Renderer::beginFrame() {
    stats.reset();
    renderQueue.clear();
}

void Renderer::endFrame() {
    processRenderQueue();
}

void Renderer::present() {
    platformSwapBuffers();
}

void Renderer::renderScene(Scene& scene) {
    setupCamera();
    
    renderQueue.clear();
    
    if (scene.getRootNode()) {
        renderNode(*scene.getRootNode(), glm::mat4(1.0f));
    }
    
    processRenderQueue();
    
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        std::cout << "Render Stats - Draw calls: " << stats.drawCalls 
                  << ", Triangles: " << stats.triangles << std::endl;
    }
}

void Renderer::renderNode(SceneNode& node, const glm::mat4& parentTransform) {
    if (!node.isVisible() || !node.isActive()) return;
    
    glm::mat4 worldTransform = parentTransform * node.getLocalMatrix();
    
    auto meshRenderer = node.getComponent<MeshRenderer>();
    if (meshRenderer && meshRenderer->isEnabled()) {
        meshRenderer->render(*this);
    }
    
    auto modelRenderer = node.getComponent<ModelRenderer>();
    if (modelRenderer && modelRenderer->isEnabled()) {
        modelRenderer->render(*this);
    }
    
    auto textComponent = node.getComponent<TextComponent>();
    if (textComponent) {
        if (textComponent->isEnabled()) {
            textComponent->render(*this, worldTransform);
        }
    }
    
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            renderNode(*child, worldTransform);
        }
    }
}

void Renderer::renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix) {
    RenderCommand command;
    command.mesh = std::shared_ptr<Mesh>(const_cast<Mesh*>(&mesh), [](Mesh*){});
    command.material = std::shared_ptr<Material>(const_cast<Material*>(&material), [](Material*){});
    command.modelMatrix = modelMatrix;
    command.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
    
    submitRenderCommand(command);
}

void Renderer::submitRenderCommand(const RenderCommand& command) {
    renderQueue.push_back(command);
}

void Renderer::setActiveCamera(CameraComponent* camera) {
    activeCamera = camera;
}

void Renderer::setViewport(int x, int y, int width, int height) {
    viewport = glm::ivec4(x, y, width, height);
    glViewport(x, y, width, height);
    
    if (activeCamera && width > 0 && height > 0) {
        activeCamera->setAspectRatio((float)width / (float)height);
    }
}

void Renderer::setClearColor(const glm::vec3& color) {
    clearColor = color;
    glClearColor(color.r, color.g, color.b, 1.0f);
}

void Renderer::setClearColor(float r, float g, float b) {
    setClearColor(glm::vec3(r, g, b));
}

void Renderer::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::setWireframe(bool enabled) {
    wireframeEnabled = enabled;
    if (enabled) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void Renderer::setDepthTest(bool enabled) {
    depthTestEnabled = enabled;
    if (enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::setCullFace(bool enabled) {
    cullFaceEnabled = enabled;
    if (enabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void Renderer::processRenderQueue() {

    // Linux builds use the full shader system
    // Sort render queue by material/shader to minimize state changes
    std::sort(renderQueue.begin(), renderQueue.end(), 
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.material.get() < b.material.get();
        });
    
    for (const auto& command : renderQueue) {
        if (command.mesh && command.material) {
            if (activeCamera) {
                glm::mat4 viewMatrix = activeCamera->getViewMatrix();
                glm::vec3 cameraPos = extractCameraPosition(viewMatrix);
                command.material->setCameraPosition(cameraPos);
            }
            
            applyMaterial(*command.material);
            
            auto shader = command.material->getShader();
            if (shader && activeCamera) {
                shader->setMat4("modelMatrix", command.modelMatrix);
                shader->setMat3("normalMatrix", command.normalMatrix);
                shader->setMat4("viewMatrix", activeCamera->getViewMatrix());
                shader->setMat4("projectionMatrix", activeCamera->getProjectionMatrix());
            }
            
            command.mesh->draw();
            
            stats.drawCalls++;
            stats.triangles += command.mesh->getTriangleCount();
            stats.vertices += command.mesh->getVertexCount();
        }
    }
}

void Renderer::setupCamera() {
    if (!activeCamera) return;
}

void Renderer::applyMaterial(const Material& material) {
    material.apply();
}

void Renderer::updateLightingUniforms() {
    auto& lightingManager = LightingManager::getInstance();
    lightingManager.update();
}

glm::vec3 Renderer::extractCameraPosition(const glm::mat4& viewMatrix) {
    return glm::vec3(viewMatrix[3][0], viewMatrix[3][1], viewMatrix[3][2]);
}

} // namespace GameEngine
