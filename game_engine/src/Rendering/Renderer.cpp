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
#include <iomanip>
#include <limits>

namespace GameEngine {

Renderer::Renderer()
    : activeCamera(nullptr)
    , viewport(0, 0, 800, 600)
    , clearColor(0.2f, 0.3f, 0.3f)
    , wireframeEnabled(false)
    , depthTestEnabled(true)
    , cullFaceEnabled(true)  // Enabled by default for better performance
    , frustumCullingEnabled(true)  // Enabled by default, but can be disabled for debugging
    , matricesCached(false)
    , frustumPlanes(6)
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
    
    if (activeCamera) {
        updateFrustum();
    }
    
    renderQueue.clear();
    
    if (scene.getRootNode()) {
        renderNode(*scene.getRootNode(), glm::mat4(1.0f));
    }
    
    processRenderQueue();
    
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        float cullPercent = 0.0f;
        if (stats.totalObjectsTested > 0) {
            cullPercent = (stats.culledObjects * 100.0f) / stats.totalObjectsTested;
        }
        
        std::cout << "Render Stats - Draw calls: " << stats.drawCalls 
                  << ", Triangles: " << stats.triangles;
        
        if (frustumCullingEnabled && stats.totalObjectsTested > 0) {
            std::cout << ", Frustum culled: " << stats.culledObjects 
                      << "/" << stats.totalObjectsTested 
                      << " (" << std::fixed << std::setprecision(1) << cullPercent << "%)";
        }
        
        std::cout << std::endl;
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
    std::sort(renderQueue.begin(), renderQueue.end(), 
        [](const RenderCommand& a, const RenderCommand& b) {
            if (!a.material && !b.material) return false;
            if (!a.material) return false;
            if (!b.material) return true;
            
            auto shaderA = a.material->getShader();
            auto shaderB = b.material->getShader();
            
            if (shaderA != shaderB) {
                return shaderA.get() < shaderB.get();
            }
            
            return a.material.get() < b.material.get();
        });
    
    glm::vec3 cameraPos(0.0f);
    if (activeCamera) {
        cachedViewMatrix = activeCamera->getViewMatrix();
        cachedProjectionMatrix = activeCamera->getProjectionMatrix();
        cameraPos = extractCameraPosition(cachedViewMatrix);
        matricesCached = true;
        
    }
    
    for (const auto& command : renderQueue) {
        if (!command.mesh) continue;
        
        if (frustumCullingEnabled && activeCamera && frustumPlanes.size() == 6) {
            glm::vec3 boundsMin = command.mesh->getBoundsMin();
            glm::vec3 boundsMax = command.mesh->getBoundsMax();
            
            bool boundsValid = (boundsMin.x < boundsMax.x && boundsMin.y < boundsMax.y && boundsMin.z < boundsMax.z);
            
            if (boundsValid) {
                const float maxVal = std::numeric_limits<float>::max();
                const float minVal = std::numeric_limits<float>::lowest();
                if (boundsMin.x > maxVal * 0.1f || boundsMax.x < minVal * 0.1f) {
                    boundsValid = false;
                }
            }
            
            if (boundsValid) {
                stats.totalObjectsTested++;
                if (!isAABBInFrustum(boundsMin, boundsMax, command.modelMatrix)) {
                    stats.culledObjects++;
                    continue;
                }
            }
        }
        
        std::shared_ptr<Material> material = command.material;
        if (!material) {
            material = Material::getDefaultMaterial();
        }
        
        bool shouldDisableCulling = command.disableCulling;
        
        bool cullingWasEnabled = cullFaceEnabled;
        if (shouldDisableCulling && cullFaceEnabled) {
            glDisable(GL_CULL_FACE);
        }
        
        if (activeCamera && matricesCached) {
            material->setCameraPosition(cameraPos);
        }
        
        applyMaterial(*material);
        
        auto shader = material->getShader();
        if (shader && activeCamera && matricesCached) {
            shader->setMat4("modelMatrix", command.modelMatrix);
            shader->setMat3("normalMatrix", command.normalMatrix);
            shader->setMat4("viewMatrix", cachedViewMatrix);
            shader->setMat4("projectionMatrix", cachedProjectionMatrix);
            
            if (!command.boneTransforms.empty()) {
                shader->setMat4Array("u_BoneMatrices", command.boneTransforms.data(), command.boneTransforms.size());
                shader->setInt("u_NumBones", static_cast<int>(command.boneTransforms.size()));
            } else {
                shader->setInt("u_NumBones", 0);
            }
        }
        
        command.mesh->draw();
        
        if (shouldDisableCulling && cullingWasEnabled) {
            glEnable(GL_CULL_FACE);
        }
        
        stats.drawCalls++;
        stats.triangles += command.mesh->getTriangleCount();
        stats.vertices += command.mesh->getVertexCount();
    }
    
    matricesCached = false;
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
    glm::mat4 invView = glm::inverse(viewMatrix);
    return glm::vec3(invView[3]);
}

void Renderer::updateFrustum() {
    if (!activeCamera) {
        frustumPlanes.clear();
        return;
    }
    
    glm::mat4 viewMatrix = activeCamera->getViewMatrix();
    glm::mat4 projMatrix = activeCamera->getProjectionMatrix();
    
    glm::mat4 viewProj = projMatrix * viewMatrix;
    
    frustumPlanes[0].normal.x = viewProj[0][3] + viewProj[0][0];
    frustumPlanes[0].normal.y = viewProj[1][3] + viewProj[1][0];
    frustumPlanes[0].normal.z = viewProj[2][3] + viewProj[2][0];
    frustumPlanes[0].distance = viewProj[3][3] + viewProj[3][0];
    
    frustumPlanes[1].normal.x = viewProj[0][3] - viewProj[0][0];
    frustumPlanes[1].normal.y = viewProj[1][3] - viewProj[1][0];
    frustumPlanes[1].normal.z = viewProj[2][3] - viewProj[2][0];
    frustumPlanes[1].distance = viewProj[3][3] - viewProj[3][0];
    
    frustumPlanes[2].normal.x = viewProj[0][3] + viewProj[0][1];
    frustumPlanes[2].normal.y = viewProj[1][3] + viewProj[1][1];
    frustumPlanes[2].normal.z = viewProj[2][3] + viewProj[2][1];
    frustumPlanes[2].distance = viewProj[3][3] + viewProj[3][1];
    
    frustumPlanes[3].normal.x = viewProj[0][3] - viewProj[0][1];
    frustumPlanes[3].normal.y = viewProj[1][3] - viewProj[1][1];
    frustumPlanes[3].normal.z = viewProj[2][3] - viewProj[2][1];
    frustumPlanes[3].distance = viewProj[3][3] - viewProj[3][1];
    
    frustumPlanes[4].normal.x = viewProj[0][3] + viewProj[0][2];
    frustumPlanes[4].normal.y = viewProj[1][3] + viewProj[1][2];
    frustumPlanes[4].normal.z = viewProj[2][3] + viewProj[2][2];
    frustumPlanes[4].distance = viewProj[3][3] + viewProj[3][2];
    
    frustumPlanes[5].normal.x = viewProj[0][3] - viewProj[0][2];
    frustumPlanes[5].normal.y = viewProj[1][3] - viewProj[1][2];
    frustumPlanes[5].normal.z = viewProj[2][3] - viewProj[2][2];
    frustumPlanes[5].distance = viewProj[3][3] - viewProj[3][2];
    
    const float epsilon = 0.0001f;
    for (auto& plane : frustumPlanes) {
        float length = glm::length(plane.normal);
        if (length > epsilon) {
            plane.normal /= length;
            plane.distance /= length;
        }
    }
}

bool Renderer::isMeshInFrustum(const Mesh& mesh, const glm::mat4& modelMatrix) const {
    if (frustumPlanes.empty() || frustumPlanes.size() != 6) {
        return true;
    }
    
    glm::vec3 boundsMin = mesh.getBoundsMin();
    glm::vec3 boundsMax = mesh.getBoundsMax();
    
    if (boundsMin.x >= boundsMax.x || boundsMin.y >= boundsMax.y || boundsMin.z >= boundsMax.z) {
        return true;
    }
    
    return isAABBInFrustum(boundsMin, boundsMax, modelMatrix);
}

bool Renderer::isAABBInFrustum(const glm::vec3& min, const glm::vec3& max, const glm::mat4& transform) const {
    if (frustumPlanes.empty() || frustumPlanes.size() != 6) {
        return true;
    }
    
    if (min.x >= max.x || min.y >= max.y || min.z >= max.z) {
        return true;
    }
    
    glm::vec3 corners[8];
    corners[0] = glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f));
    corners[1] = glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f));
    corners[2] = glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f));
    corners[3] = glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f));
    corners[4] = glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f));
    corners[5] = glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f));
    corners[6] = glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f));
    corners[7] = glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f));
    
    for (const auto& plane : frustumPlanes) {
        bool inside = false;
        
        const float margin = -0.1f;
        for (int i = 0; i < 8; i++) {
            float distance = glm::dot(plane.normal, corners[i]) + plane.distance;
            if (distance > margin) {
                inside = true;
                break;
            }
        }
        
        if (!inside) {
            return false;
        }
    }
    
    return true;
}

} // namespace GameEngine
