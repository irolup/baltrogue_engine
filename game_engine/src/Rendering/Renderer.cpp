#include "Rendering/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/BeamRenderer.h"
#include "Components/TextComponent.h"
#include "Components/SkyboxComponent.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"
#include "Rendering/LightingManager.h"
#include "Rendering/Material.h"
#include "Physics/PhysicsManager.h"
#include "Core/Engine.h"
#include "Platform.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <limits>

namespace GameEngine {

Renderer::Renderer()
    : activeCamera(nullptr)
    , currentScene(nullptr)
    , viewport(0, 0, VITA_WIDTH, VITA_HEIGHT)
    , framebufferWidth(VITA_WIDTH)
    , framebufferHeight(VITA_HEIGHT)
    , clearColor(0.2f, 0.3f, 0.3f)
    , wireframeEnabled(false)
    , depthTestEnabled(true)
    , cullFaceEnabled(true)
    , frustumCullingEnabled(true)
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

void Renderer::syncViewportToFramebuffer() {
    int w = VITA_WIDTH;
    int h = VITA_HEIGHT;
#ifdef LINUX_BUILD
    if (window) {
        glfwGetFramebufferSize(window, &w, &h);
    }
#endif
    if (w <= 0 || h <= 0) {
        return;
    }
    if (framebufferWidth != w && framebufferHeight != h) {
        framebufferWidth = w;
        framebufferHeight = h;
        viewport = glm::ivec4(0, 0, w, h);
        glViewport(0, 0, w, h);
    }
}

void Renderer::beginFrame() {
    syncViewportToFramebuffer();
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
    currentScene = &scene;
    setupCamera();
    
    if (activeCamera) {
        updateFrustum(activeCamera->getViewMatrix(), activeCamera->getProjectionMatrix());
    }
    
    renderQueue.clear();
    
    if (scene.getRootNode()) {
        renderNode(*scene.getRootNode(), glm::mat4(1.0f));
    }
    
    renderSkybox(scene);
    processRenderQueue();

    if (scene.getRootNode()) {
        renderTextNodes(*scene.getRootNode(), glm::mat4(1.0f));
    }
    
    if (PhysicsManager::getInstance().isDebugDrawEnabled() && activeCamera) {
        glm::mat4 viewMat = activeCamera->getViewMatrix();
        glm::mat4 projMat = activeCamera->getProjectionMatrix();
        auto debugMaterial = std::make_shared<Material>();
        debugMaterial->setColor(glm::vec3(0.0f, 1.0f, 0.0f));
        auto debugShader = std::make_shared<Shader>();
        const char* vertexSource = "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "uniform mat4 modelMatrix;\n"
            "uniform mat4 viewMatrix;\n"
            "uniform mat4 projectionMatrix;\n"
            "void main() { gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0); }\n";
        const char* fragmentSource = "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 u_Color;\n"
            "void main() { FragColor = vec4(u_Color, 1.0); }\n";
        if (debugShader->loadFromSource(vertexSource, fragmentSource)) {
            debugMaterial->setShader(debugShader);
            debugMaterial->setColor(glm::vec3(0.0f, 1.0f, 0.0f));
            GLint savedPolygonMode[2];
            glGetIntegerv(GL_POLYGON_MODE, savedPolygonMode);
            GLboolean savedDepthTest;
            glGetBooleanv(GL_DEPTH_TEST, &savedDepthTest);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_DEPTH_TEST);
            PhysicsManager::getInstance().renderDebugShapes(*debugMaterial, viewMat, projMat);
            glPolygonMode(GL_FRONT_AND_BACK, savedPolygonMode[0]);
            if (savedDepthTest) glEnable(GL_DEPTH_TEST);
        }
    }
    
    currentScene = nullptr;
    
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

    auto beamRenderer = node.getComponent<BeamRenderer>();
    if (beamRenderer && beamRenderer->isEnabled()) {
        beamRenderer->render(*this);
    }
    
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            renderNode(*child, worldTransform);
        }
    }
}

void Renderer::renderFromCamera(Scene& scene, CameraComponent* cam, const glm::vec4& vpNorm) {
    if (!cam || vpNorm.x < 0.0f || vpNorm.y < 0.0f || vpNorm.z <= 0.0f || vpNorm.w <= 0.0f) return;

    int fbW = framebufferWidth > 0 ? framebufferWidth : viewport.z;
    int fbH = framebufferHeight > 0 ? framebufferHeight : viewport.w;
    if (fbW <= 0 || fbH <= 0) return;

    int px = static_cast<int>(vpNorm.x * static_cast<float>(fbW));
    int py = static_cast<int>(vpNorm.y * static_cast<float>(fbH));
    int pw = static_cast<int>(vpNorm.z * static_cast<float>(fbW));
    int ph = static_cast<int>(vpNorm.w * static_cast<float>(fbH));
    if (pw <= 0 || ph <= 0) return;

    const glm::ivec4 newViewport(px, py, pw, ph);

    // Save state
    const glm::ivec4 oldViewport = viewport;
    CameraComponent* oldCamera = activeCamera;

    // Change viewport ONLY if needed
    if (newViewport != viewport) {
        glViewport(px, py, pw, ph);
        viewport = newViewport;
    }

    // Update aspect ratio ONLY if changed
    const float newAspect = static_cast<float>(pw) / static_cast<float>(ph);
    if (cam->getAspectRatio() != newAspect) {
        cam->setAspectRatio(newAspect);
    }

    activeCamera = cam;

    glClear(GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = cam->getViewMatrix();
    glm::mat4 proj = cam->getProjectionMatrix();
    updateFrustum(view, proj);

    currentScene = &scene;
    renderQueue.clear();
    if (scene.getRootNode()) renderNode(*scene.getRootNode(), glm::mat4(1.0f));
    renderSkybox(scene);
    cachedViewMatrix = view;
    cachedProjectionMatrix = proj;
    matricesCached = true;
    processRenderQueue();
    if (scene.getRootNode()) renderTextNodes(*scene.getRootNode(), glm::mat4(1.0f));

    currentScene = nullptr;

    // Restore previous viewport ONLY if needed
    if (oldViewport != viewport) {
        glViewport(oldViewport.x, oldViewport.y, oldViewport.z, oldViewport.w);
        viewport = oldViewport;
    }
    activeCamera = oldCamera;
}

void Renderer::renderTextNodes(SceneNode& node, const glm::mat4& parentTransform) {
    if (!node.isVisible() || !node.isActive()) return;
    glm::mat4 worldTransform = parentTransform * node.getLocalMatrix();
    auto textComponent = node.getComponent<TextComponent>();
    if (textComponent && textComponent->isEnabled()) {
        textComponent->render(*this, worldTransform);
    }
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            renderTextNodes(*child, worldTransform);
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
            bool aOpaque = (a.material->getBlendMode() == BlendMode::Opaque);
            bool bOpaque = (b.material->getBlendMode() == BlendMode::Opaque);
            if (aOpaque != bOpaque)
                return aOpaque;
            auto shaderA = a.material->getShader();
            auto shaderB = b.material->getShader();
            if (shaderA != shaderB)
                return shaderA.get() < shaderB.get();
            return a.material.get() < b.material.get();
        });
    
    glm::vec3 cameraPos(0.0f);
    if (activeCamera && !matricesCached) {
        cachedViewMatrix = activeCamera->getViewMatrix();
        cachedProjectionMatrix = activeCamera->getProjectionMatrix();
        matricesCached = true;
    }
    if (matricesCached) {
        cameraPos = extractCameraPosition(cachedViewMatrix);
    }

    for (const auto& command : renderQueue) {
        if (!command.mesh) continue;
        
        if (!command.isBeam && frustumCullingEnabled && activeCamera && frustumPlanes.size() == 6) {
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
        if (!material) continue;
        
        bool shouldDisableCulling = command.disableCulling;
        
        bool cullingWasEnabled = cullFaceEnabled;
        if (shouldDisableCulling && cullFaceEnabled) {
            glDisable(GL_CULL_FACE);
        }
        
        if (activeCamera && matricesCached) {
            material->setCameraPosition(cameraPos);
        }
        
        auto shader = material->getShader();
        bool isDefaultLit = shader && (shader == Shader::getLightingShader());
        if (isDefaultLit && currentScene) {
            auto activeSkyboxNode = currentScene->getActiveSkybox();
            if (activeSkyboxNode) {
                auto skyboxComp = activeSkyboxNode->getComponent<SkyboxComponent>();
                if (skyboxComp && skyboxComp->isActive()) {
                    auto envMap = skyboxComp->getCubemapTexture();
                    if (envMap) {
                        material->setTexture("u_EnvironmentMap", envMap);
                        material->setBool("u_HasEnvironmentMap", true);
                    } else {
                        material->setBool("u_HasEnvironmentMap", false);
                    }
                } else {
                    material->setBool("u_HasEnvironmentMap", false);
                }
            } else {
                material->setBool("u_HasEnvironmentMap", false);
            }
        }
        
        applyMaterial(*material);
        
        if (shader && activeCamera && matricesCached) {
            if (command.isBeam) {
                shader->setMat4("viewMatrix", cachedViewMatrix);
                shader->setMat4("projectionMatrix", cachedProjectionMatrix);
                shader->setVec3("u_BeamStart", command.beamStart);
                shader->setVec3("u_BeamEnd", command.beamEnd);
                shader->setFloat("u_BeamHalfWidth", command.beamHalfWidth);
                shader->setVec3("u_CameraPos", cameraPos);
                shader->setFloat("u_Time", GetEngine().getTime().getTotalTime());
            } else {
                shader->setMat4("modelMatrix", command.modelMatrix);
                shader->setMat3("normalMatrix", command.normalMatrix);
                shader->setMat4("viewMatrix", cachedViewMatrix);
                shader->setMat4("projectionMatrix", cachedProjectionMatrix);
                shader->setFloat("u_Time", GetEngine().getTime().getTotalTime());
                shader->setVec2("u_ViewportSize", glm::vec2(viewport.z, viewport.w));
                
                if (!command.boneTransforms.empty()) {
                    shader->setMat4Array("u_BoneMatrices", command.boneTransforms.data(), command.boneTransforms.size());
                    shader->setInt("u_NumBones", static_cast<int>(command.boneTransforms.size()));
                } else {
                    shader->setInt("u_NumBones", 0);
                }
            }
        }
        
        command.mesh->draw();
        
        if (!material->getDepthWrite()) {
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
        }
        
        if (shouldDisableCulling && cullingWasEnabled) {
            glEnable(GL_CULL_FACE);
        }
        
        stats.drawCalls++;
        stats.triangles += command.mesh->getTriangleCount();
        stats.vertices += command.mesh->getVertexCount();
    }
    
    matricesCached = false;
    renderQueue.clear();
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

void Renderer::updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
    if (!activeCamera) {
        frustumPlanes.clear();
        return;
    }
    
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

void Renderer::renderSkybox(Scene& scene) {
    auto activeSkyboxNode = scene.getActiveSkybox();
    if (!activeSkyboxNode) return;
    
    auto skyboxComp = activeSkyboxNode->getComponent<SkyboxComponent>();
    if (!skyboxComp || !skyboxComp->isActive()) return;
    
    auto cubemapTexture = skyboxComp->getCubemapTexture();
    auto skyboxMesh = skyboxComp->getSkyboxMesh();
    auto skyboxMaterial = skyboxComp->getSkyboxMaterial();
    
    if (!cubemapTexture || !skyboxMesh || !skyboxMaterial || !activeCamera) return;
    
    bool cullingWasEnabled = cullFaceEnabled;
    if (cullFaceEnabled) {
        glDisable(GL_CULL_FACE);
    }
    
    glm::mat4 viewMatrix = glm::mat4(glm::mat3(activeCamera->getViewMatrix()));
    
    glm::mat4 projectionMatrix = activeCamera->getProjectionMatrix();
    
    skyboxMaterial->apply();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    
    auto shader = skyboxMaterial->getShader();
    if (shader && shader->isValid()) {
        shader->use();
        shader->setMat4("view", viewMatrix);
        shader->setMat4("projection", projectionMatrix);
        shader->setInt("skybox", 0);
        
        cubemapTexture->bindCubemap(0);
    }
    
    skyboxMesh->bind();
    skyboxMesh->draw();
    skyboxMesh->unbind();
    
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    
    if (cullingWasEnabled && cullFaceEnabled) {
        glEnable(GL_CULL_FACE);
    }
    
    stats.drawCalls++;
    stats.triangles += skyboxMesh->getTriangleCount();
}

} // namespace GameEngine
