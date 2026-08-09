#include "Rendering/OpenGLRenderer.h"
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
#include "Rendering/ShadowMap.h"
#include "Physics/PhysicsManager.h"
#include "Core/Engine.h"
#include "Platform.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
namespace GameEngine {

OpenGLRenderer::OpenGLRenderer()
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
{
}

// OpenGLRenderer::~Renderer() {
//     shutdown();
// }

bool OpenGLRenderer::initialize()  {
    if (depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    
    if (cullFaceEnabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    
    return true;
}

void OpenGLRenderer::shutdown() {
}

void OpenGLRenderer::syncViewportToFramebuffer() {
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
    if (framebufferWidth != w || framebufferHeight != h) {
        framebufferWidth = w;
        framebufferHeight = h;
        viewport = glm::ivec4(0, 0, w, h);
        glViewport(0, 0, w, h);
    }
}

void OpenGLRenderer::beginFrame() {
    syncViewportToFramebuffer();
    stats.reset();
    renderQueue.clear();
}

void OpenGLRenderer::endFrame() {
    processRenderQueue();
}

void OpenGLRenderer::present() {
    platformSwapBuffers();
}

void OpenGLRenderer::renderScene(Scene& scene) {
    currentScene = &scene;
    setupCamera();

    LightingManager::getInstance().beginPass();

    if (activeCamera) {
        updateFrustum(activeCamera->getViewMatrix(), activeCamera->getProjectionMatrix());
        ShadowManager::getInstance().update(
            extractCameraPosition(activeCamera->getViewMatrix()),
            activeCamera->getForward());
    } else {
        ShadowManager::getInstance().clear();
    }

    renderQueue.clear();

    if (scene.getRootNode()) {
        renderNode(*scene.getRootNode(), glm::mat4(1.0f));
    }

    renderShadowPass();
    bindShadowResources();

    renderSkybox(scene);
    processRenderQueue();

    if (scene.getRootNode()) {
        renderTextNodes(*scene.getRootNode(), glm::mat4(1.0f));
    }
    
    if (PhysicsManager::getInstance().isDebugDrawEnabled() && activeCamera) {
        glm::mat4 viewMat = activeCamera->getViewMatrix();
        glm::mat4 projMat = activeCamera->getProjectionMatrix();
        static std::shared_ptr<Material> debugMaterial;
        static bool debugShaderReady = false;
        if (!debugMaterial) {
            debugMaterial = std::make_shared<Material>();
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
            debugShaderReady = debugShader->loadFromSource(vertexSource, fragmentSource);
            if (debugShaderReady) {
                debugMaterial->setShader(debugShader);
                debugMaterial->setColor(glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
        if (debugShaderReady) {
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

#ifdef DEBUG
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
#endif
}

void OpenGLRenderer::renderNode(SceneNode& node, const glm::mat4& parentTransform) {
    if (!node.isVisible() || !node.isActive()) return;
    
    glm::mat4 worldTransform = parentTransform * node.getLocalMatrix();

    const auto& components = node.getAllComponents();
    for (const auto& component : components) {
        if (!component || !component->isEnabled()) {
            continue;
        }
        const std::string& typeName = component->getTypeName();
        if (typeName == MeshRenderer::StaticTypeName()) {
            static_cast<MeshRenderer*>(component.get())->render(*this);
        } else if (typeName == ModelRenderer::StaticTypeName()) {
            static_cast<ModelRenderer*>(component.get())->render(*this);
        } else if (typeName == BeamRenderer::StaticTypeName()) {
            static_cast<BeamRenderer*>(component.get())->render(*this);
        }
    }
    
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            renderNode(*child, worldTransform);
        }
    }
}

void OpenGLRenderer::renderFromCamera(Scene& scene, CameraComponent* cam, const glm::vec4& vpNorm) {
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

    LightingManager::getInstance().beginPass();
    ShadowManager::getInstance().update(extractCameraPosition(view), cam->getForward());

    currentScene = &scene;
    renderQueue.clear();
    if (scene.getRootNode()) renderNode(*scene.getRootNode(), glm::mat4(1.0f));

    renderShadowPass();
    bindShadowResources();

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

void OpenGLRenderer::renderTextNodes(SceneNode& node, const glm::mat4& parentTransform) {
    if (!node.isVisible() || !node.isActive()) return;
    glm::mat4 worldTransform = parentTransform * node.getLocalMatrix();
    for (const auto& component : node.getAllComponents()) {
        if (component && component->isEnabled()
            && component->getTypeName() == TextComponent::StaticTypeName()) {
            static_cast<TextComponent*>(component.get())->render(*this, worldTransform);
        }
    }
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            renderTextNodes(*child, worldTransform);
        }
    }
}

void OpenGLRenderer::renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix) {
    RenderCommand command;
    command.mesh = std::shared_ptr<Mesh>(const_cast<Mesh*>(&mesh), [](Mesh*){});
    command.material = std::shared_ptr<Material>(const_cast<Material*>(&material), [](Material*){});
    command.modelMatrix = modelMatrix;
    command.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
    
    submitRenderCommand(command);
}

void OpenGLRenderer::submitRenderCommand(const RenderCommand& command) {
    renderQueue.push_back(command);
}

void OpenGLRenderer::setActiveCamera(CameraComponent* camera) {
    activeCamera = camera;
}

void OpenGLRenderer::setViewport(int x, int y, int width, int height) {
    viewport = glm::ivec4(x, y, width, height);
    glViewport(x, y, width, height);
    
    if (activeCamera && width > 0 && height > 0) {
        activeCamera->setAspectRatio((float)width / (float)height);
    }
}

void OpenGLRenderer::setClearColor(const glm::vec3& color) {
    clearColor = color;
    glClearColor(color.r, color.g, color.b, 1.0f);
}

void OpenGLRenderer::setClearColor(float r, float g, float b) {
    setClearColor(glm::vec3(r, g, b));
}

void OpenGLRenderer::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::setWireframe(bool enabled) {
    wireframeEnabled = enabled;
    if (enabled) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void OpenGLRenderer::setDepthTest(bool enabled) {
    depthTestEnabled = enabled;
    if (enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void OpenGLRenderer::setCullFace(bool enabled) {
    cullFaceEnabled = enabled;
    if (enabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void OpenGLRenderer::processRenderQueue() {
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
        
        if (!command.isBeam && frustumCullingEnabled && activeCamera) {
            const glm::vec3 boundsMin = command.mesh->getBoundsMin();
            const glm::vec3 boundsMax = command.mesh->getBoundsMax();

            if (Frustum::areBoundsValid(boundsMin, boundsMax)) {
                stats.totalObjectsTested++;
                if (!cameraFrustum.containsAABB(boundsMin, boundsMax, command.modelMatrix)) {
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
                shader->setInt("u_ReceiveShadows",
                    (shadowAtlasReady && command.receiveShadows) ? 1 : 0);
                
                if (command.boneTransforms && !command.boneTransforms->empty()) {
                    shader->setMat4Array("u_BoneMatrices", command.boneTransforms->data(), command.boneTransforms->size());
                    shader->setInt("u_NumBones", static_cast<int>(command.boneTransforms->size()));
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

void OpenGLRenderer::renderShadowPass() {
#ifndef ENABLE_VULKAN
    shadowAtlasReady = false;

    auto& shadowManager = ShadowManager::getInstance();
    const std::vector<ShadowView>& views = shadowManager.getViews();

    if (views.empty()) {
        return;
    }

    // Nothing in this scene casts, so the atlas would come out empty
    bool hasCaster = false;
    for (const auto& command : renderQueue) {
        if (command.mesh && command.castShadows && !command.isBeam) {
            hasCaster = true;
            break;
        }
    }
    if (!hasCaster) {
        return;
    }

    auto shadowShader = Shader::getShadowDepthShader();
    if (!shadowShader || !shadowShader->isValid()) {
        return;
    }

    if (!shadowAtlas.ensureCreated(shadowManager.getAtlasWidth(), shadowManager.getAtlasHeight())) {
        return;
    }

    // The editor renders the scene into its own framebuffer, so restore whatever
    // was bound rather than assuming the default one
    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    shadowAtlas.bindForWriting();

    // Depth must be written and tested regardless of what the lit pass wants
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    if (!depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
#ifdef VITA_BUILD
    // The Vita packs depth into the colour attachment, so it has to be cleared
    // to "furthest" (1.0 packs to white) as well as the depth buffer
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
#else
    glClear(GL_DEPTH_BUFFER_BIT);
#endif

    shadowShader->use();
    shadowShader->setFloat("u_AttributeGuard", 0.0f);

    for (size_t viewIndex = 0; viewIndex < views.size(); ++viewIndex) {
        const ShadowView& view = views[viewIndex];
        const glm::ivec4 tile = shadowManager.getTileViewport(view.tile);
        glViewport(tile.x, tile.y, tile.z, tile.w);

        shadowShader->setMat4("u_LightViewProj", view.viewProjection);

        for (const auto& command : renderQueue) {
            if (!command.mesh || !command.castShadows || command.isBeam) {
                continue;
            }

            const glm::vec3 boundsMin = command.mesh->getBoundsMin();
            const glm::vec3 boundsMax = command.mesh->getBoundsMax();
            if (Frustum::areBoundsValid(boundsMin, boundsMax)
                && !view.frustum.containsAABB(boundsMin, boundsMax, command.modelMatrix)) {
                continue;
            }

            shadowShader->setMat4("modelMatrix", command.modelMatrix);
            if (command.boneTransforms && !command.boneTransforms->empty()) {
                shadowShader->setMat4Array("u_BoneMatrices", command.boneTransforms->data(), command.boneTransforms->size());
                shadowShader->setInt("u_NumBones", static_cast<int>(command.boneTransforms->size()));
            } else {
                shadowShader->setInt("u_NumBones", 0);
            }

            command.mesh->draw();
            stats.drawCalls++;
            stats.triangles += command.mesh->getTriangleCount();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    if (!depthTestEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    shadowAtlasReady = true;
#endif
}

void OpenGLRenderer::bindShadowResources() {
#ifndef ENABLE_VULKAN

    if (shadowAtlasReady) {
        shadowAtlas.bindTexture(kShadowMapTextureUnit);
    } else if (auto white = Texture::getWhiteTexture()) {
        white->bind(kShadowMapTextureUnit);
    }
    glActiveTexture(GL_TEXTURE0);

    LightingManager::getInstance().setShadowMapBound(shadowAtlasReady);
#endif
}

void OpenGLRenderer::setupCamera() {
    if (!activeCamera) return;
}

void OpenGLRenderer::applyMaterial(const Material& material) {
    material.apply();
}

void OpenGLRenderer::updateLightingUniforms() {
    auto& lightingManager = LightingManager::getInstance();
    lightingManager.update();
}

glm::vec3 OpenGLRenderer::extractCameraPosition(const glm::mat4& viewMatrix) {
    glm::mat4 invView = glm::inverse(viewMatrix);
    return glm::vec3(invView[3]);
}

void OpenGLRenderer::updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projMatrix) {
    if (!activeCamera) {
        return;
    }

    cameraFrustum.update(projMatrix * viewMatrix);
}

void OpenGLRenderer::renderSkybox(Scene& scene) {
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

}
