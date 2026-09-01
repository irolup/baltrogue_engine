#include "Components/TextComponent.h"
#include "Rendering/Renderer.h"
#ifdef ENABLE_VULKAN
#include "Core/Engine.h"
#include "Rendering/Vulkan/VulkanResources.h"
#endif
#include "Rendering/FontManager.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"
#include "Components/CameraComponent.h"
#include "Scene/SceneNode.h"
#include "Rendering/Mesh.h"
#include "Rendering/TextMaterial.h"
#include <iostream>
#include <algorithm>
#include <map>

#ifdef ENABLE_VULKAN
#include "Rendering/Vulkan/VulkanConfig.h"
#endif

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

uint32_t TextMaterial::nextRevision = 0;

// Static maps for editor UI buffers - declared at namespace level to persist
static std::map<TextComponent*, std::string> textBuffers;
static std::map<TextComponent*, std::string> fontPathBuffers;

TextComponent::TextComponent()
    : text("Hello World")
    , fontPath("assets/fonts/DroidSans.ttf")
    , fontSize(32.0f)
    , color(1.0f, 1.0f, 1.0f, 1.0f)
    , alignment(TextAlignment::LEFT)
    , renderMode(TextRenderMode::WORLD_SPACE)
    , scale(1.0f)
    , lineSpacing(1.2f)
    , atlasWidth(1024)
    , atlasHeight(1024)
    , charsToInclude(95)
    , firstCharCodePoint(32)
    , vao(0)
    , vbo(0)
    , ebo(0)
    , needsUpdate(true)
    , isInitialized(false)
    , textMeshRevision(0)
{
}

TextComponent::~TextComponent() {
    cleanupBuffers();
    
#ifdef EDITOR_BUILD
    // Clean up static buffers for this component instance
    textBuffers.erase(this);
    fontPathBuffers.erase(this);
#endif
}

void TextComponent::start() {
    if (isInitialized) {
        return;
    }

    initializeFont();

#ifndef ENABLE_VULKAN
    setupBuffers();
#endif

    isInitialized = true;

    if (needsUpdate) {
        updateTextMesh();
        needsUpdate = false;
    }
}

void TextComponent::update(float deltaTime) {
    if (needsUpdate && isInitialized) {
        updateTextMesh();
        needsUpdate = false;
    }
}

void TextComponent::render(IRenderer& renderer) {
#ifdef ENABLE_VULKAN
    if (!isInitialized || text.empty() || !textMaterial)
    {
        return;
    }
#else
    if (!isInitialized || text.empty() || !fontAtlasTexture)
    {
        return;
    }
#endif
#ifdef ENABLE_VULKAN

    TextRenderCommand cmd;
    cmd.mesh = textMesh;
    cmd.material = textMaterial;
    cmd.color = color;
    cmd.renderMode = renderMode;
    cmd.modelMatrix = buildClipMatrix(renderer, owner ? owner->getWorldMatrix() : glm::mat4(1.0f));
    cmd.textComponent = this;

    renderer.submitTextRenderCommand(cmd);
#else
    
    if (renderMode == TextRenderMode::WORLD_SPACE) {
        renderWorldSpace(renderer);
    } else {
        renderScreenSpace(renderer);
    }
#endif
}

void TextComponent::render(IRenderer& renderer, const glm::mat4& worldTransform) {
#ifdef ENABLE_VULKAN
    if (!isInitialized || text.empty() || !textMaterial)
    {
        return;
    }
#else
    if (!isInitialized || text.empty() || !fontAtlasTexture)
    {
        return;
    }
#endif

#ifdef ENABLE_VULKAN

    TextRenderCommand cmd;
    cmd.mesh = textMesh;
    cmd.material = textMaterial;
    cmd.color = color;
    cmd.renderMode = renderMode;
    cmd.modelMatrix = buildClipMatrix(renderer, worldTransform);
    cmd.textComponent = this;

    renderer.submitTextRenderCommand(cmd);
#else
    
    if (renderMode == TextRenderMode::WORLD_SPACE) {
        // Rendering world space text
        renderWorldSpace(renderer, worldTransform);
    } else {
        // Rendering screen space text
        renderScreenSpace(renderer);
    }
#endif
}

#ifdef ENABLE_VULKAN
glm::mat4 TextComponent::buildClipMatrix(IRenderer& renderer, const glm::mat4& worldTransform) const {
    auto camera = renderer.getActiveCamera();
    if (!camera) {
        return worldTransform;
    }

    if (renderMode == TextRenderMode::SCREEN_SPACE) {
        const float aspectRatio = camera->getAspectRatio();
        const float orthoHeight = 2.0f;
        const float orthoWidth = orthoHeight * aspectRatio;

        const glm::mat4 viewMatrix = glm::mat4(1.0f);
        glm::mat4 projectionMatrix = glm::ortho(
            -orthoWidth / 2.0f, orthoWidth / 2.0f,
            -orthoHeight / 2.0f, orthoHeight / 2.0f,
            -1.0f, 1.0f);
        projectionMatrix = fixProjectionForVulkan(projectionMatrix);

        glm::mat4 modelMatrix = glm::mat4(1.0f);
        const glm::vec3 screenPos = owner ? owner->getTransform().getPosition() : glm::vec3(0.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(screenPos.x * 0.1f, screenPos.y * 0.1f, 0.0f));
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
        return projectionMatrix * viewMatrix * modelMatrix;
    }

    return camera->getProjectionMatrix() * camera->getViewMatrix() * worldTransform;
}
#endif

void TextComponent::destroy() {
    cleanupBuffers();
    cleanupFontAtlas();
#ifdef ENABLE_VULKAN
    if (Engine* engine = GetEngineIfExists()) {
        if (auto* resources = engine->getVulkanResources()) {
            resources->evictTextMesh(this);
        }
    }
    textMaterial.reset();
#endif
    isInitialized = false;
}

void TextComponent::prepareForRestart() {
    isInitialized = false;
}

void TextComponent::suspend() {
    // Keep GPU buffers alive for cached scene transitions.
}

void TextComponent::resume() {
    if (!isInitialized) {
        start();
    }
}

void TextComponent::setText(const std::string& newText) {
    if (text != newText) {
        text = newText;
        needsUpdate = true;
    }

    // Rebuild immediately when possible (pause menus call setText while game is paused).
    if (needsUpdate && isInitialized) {
        updateTextMesh();
        needsUpdate = false;
    }
}

void TextComponent::setFontPath(const std::string& path) {
    if (fontPath != path) {
        fontPath = path;
        cleanupFontAtlas();
        initializeFont();
        needsUpdate = true;
        
        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

void TextComponent::setFontSize(float size) {
    if (fontSize != size) {
        fontSize = size;
        cleanupFontAtlas();
        initializeFont();
        needsUpdate = true;
        
        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

void TextComponent::setColor(const glm::vec4& newColor) {
    if (color != newColor) {
        color = newColor;
        needsUpdate = true;

        if (textMaterial) {
            textMaterial->setColor(color);
        }

        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

void TextComponent::setAlignment(TextAlignment newAlignment) {
    if (alignment != newAlignment) {
        alignment = newAlignment;
        needsUpdate = true;
        
        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

void TextComponent::setRenderMode(TextRenderMode mode) {
    if (renderMode != mode) {
        renderMode = mode;
        needsUpdate = true;
        
        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

void TextComponent::setScale(float newScale) {
    if (scale != newScale) {
        scale = newScale;
        needsUpdate = true;
        
        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

void TextComponent::setLineSpacing(float spacing) {
    if (lineSpacing != spacing) {
        lineSpacing = spacing;
        needsUpdate = true;
        
        // In editor mode, immediately update the mesh since update() might not be called
        #ifdef EDITOR_BUILD
        if (isInitialized && needsUpdate) {
            updateTextMesh();
            needsUpdate = false;
        }
        #endif
    }
}

bool TextComponent::getScreenRect(const glm::vec2& viewportSize, glm::vec4& outRect) const {
    if (renderMode != TextRenderMode::SCREEN_SPACE || vertices.empty() || !owner ||
        viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
        return false;
    }

    glm::vec2 localMin(vertices[0].position);
    glm::vec2 localMax = localMin;
    for (const TextVertex& vertex : vertices) {
        localMin = glm::min(localMin, glm::vec2(vertex.position));
        localMax = glm::max(localMax, glm::vec2(vertex.position));
    }

    const glm::vec3 position = owner->getTransform().getPosition();
    const glm::vec2 origin(position.x * 0.1f, position.y * 0.1f);
    localMin = origin + localMin * scale;
    localMax = origin + localMax * scale;

    const float orthoHeight = 2.0f;
    const float orthoWidth = orthoHeight * (static_cast<float>(VITA_WIDTH) / static_cast<float>(VITA_HEIGHT));

    const float left = (localMin.x + orthoWidth * 0.5f) / orthoWidth * viewportSize.x;
    const float right = (localMax.x + orthoWidth * 0.5f) / orthoWidth * viewportSize.x;
    const float top = (orthoHeight * 0.5f - localMax.y) / orthoHeight * viewportSize.y;
    const float bottom = (orthoHeight * 0.5f - localMin.y) / orthoHeight * viewportSize.y;

    outRect = glm::vec4(left, top, right - left, bottom - top);
    return true;
}

glm::vec2 TextComponent::getTextBounds() const {
    return calculateTextSize();
}

float TextComponent::getTextWidth() const {
    return calculateTextSize().x;
}

float TextComponent::getTextHeight() const {
    return calculateTextSize().y;
}

void TextComponent::drawInspector() {
#ifdef EDITOR_BUILD
    // Text content - use a unique buffer per component instance
    if (textBuffers.find(this) == textBuffers.end()) {
        textBuffers[this] = text;
    }
    textBuffers[this] = text; // Update with current text
    
    char textBuffer[256];
    strncpy(textBuffer, textBuffers[this].c_str(), sizeof(textBuffer) - 1);
    textBuffer[sizeof(textBuffer) - 1] = '\0';
    
    if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(0, 60))) {
        textBuffers[this] = std::string(textBuffer);
        setText(textBuffers[this]);
    }
    
    // Font path - use a unique buffer per component instance
    if (fontPathBuffers.find(this) == fontPathBuffers.end()) {
        fontPathBuffers[this] = fontPath;
    }
    fontPathBuffers[this] = fontPath; // Update with current font path
    
    char fontPathBuffer[256];
    strncpy(fontPathBuffer, fontPathBuffers[this].c_str(), sizeof(fontPathBuffer) - 1);
    fontPathBuffer[sizeof(fontPathBuffer) - 1] = '\0';
    
    if (ImGui::InputText("Font Path", fontPathBuffer, sizeof(fontPathBuffer))) {
        fontPathBuffers[this] = std::string(fontPathBuffer);
        setFontPath(fontPathBuffers[this]);
    }
    
    // Font size
    float fontSizeValue = fontSize;
    if (ImGui::DragFloat("Font Size", &fontSizeValue, 1.0f, 8.0f, 200.0f)) {
        setFontSize(fontSizeValue);
    }
    
    // Color
    float colorArray[4] = {color.r, color.g, color.b, color.a};
    if (ImGui::ColorEdit4("Color", colorArray)) {
        setColor(glm::vec4(colorArray[0], colorArray[1], colorArray[2], colorArray[3]));
    }
    
    // Render mode
    const char* renderModeItems[] = {"World Space", "Screen Space"};
    int currentRenderMode = static_cast<int>(renderMode);
    if (ImGui::Combo("Render Mode", &currentRenderMode, renderModeItems, 2)) {
        setRenderMode(static_cast<TextRenderMode>(currentRenderMode));
    }
    
    // Alignment
    const char* alignmentItems[] = {"Left", "Center", "Right"};
    int currentAlignment = static_cast<int>(alignment);
    if (ImGui::Combo("Alignment", &currentAlignment, alignmentItems, 3)) {
        setAlignment(static_cast<TextAlignment>(currentAlignment));
    }
    
    // Scale
    float scaleValue = scale;
    if (ImGui::DragFloat("Scale", &scaleValue, 0.1f, 0.1f, 10.0f)) {
        setScale(scaleValue);
    }
    
    // Line spacing
    float lineSpacingValue = lineSpacing;
    if (ImGui::DragFloat("Line Spacing", &lineSpacingValue, 0.1f, 0.5f, 3.0f)) {
        setLineSpacing(lineSpacingValue);
    }
    
    // Text bounds info
    ImGui::Separator();
    ImGui::Text("Text Bounds: %.2f x %.2f", getTextWidth(), getTextHeight());
#endif
}

void TextComponent::initializeFont() {
    auto& fontManager = FontManager::getInstance();
    auto atlas = fontManager.loadFont(fontPath, fontSize, atlasWidth, atlasHeight, charsToInclude, firstCharCodePoint);
    
    if (atlas) {
    #ifndef ENABLE_VULKAN
        fontAtlasTexture = atlas->texture;
    #endif

        packedChars = atlas->packedChars;
        alignedQuads = atlas->alignedQuads;
        textShader = Shader::getTextShader();

    #ifdef ENABLE_VULKAN

        textMaterial = std::make_shared<TextMaterial>();
        textMaterial->fontAtlas = atlas;
        textMaterial->setColor(color);

        return;

    #endif

        if (!textShader || !textShader->isValid()) {
            std::cerr << "Failed to get shared text shader!" << std::endl;
            textShader.reset();
        }
    } else {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
    }
}

void TextComponent::updateTextMesh() {
    generateVertices(renderMode);

#ifndef ENABLE_VULKAN
    updateBuffers();
#endif

    rebuildMesh();
    ++textMeshRevision;
}

void TextComponent::setupBuffers() {
#ifdef ENABLE_VULKAN
    return;
#endif
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    
    // Setup vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TextVertex) * 1000, nullptr, GL_DYNAMIC_DRAW);
    
    // Setup element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6000, nullptr, GL_DYNAMIC_DRAW);

    #ifdef LINUX_BUILD
        GLint posLoc   = glGetAttribLocation(textShader->getProgram(), "aPosition");
        GLint texLoc   = glGetAttribLocation(textShader->getProgram(), "aTexCoord");
    #else
        GLint posLoc = glGetAttribLocation(textShader->getProgram(), "aPosition");
        GLint texLoc = glGetAttribLocation(textShader->getProgram(), "aTexCoord");
        if (posLoc < 0) posLoc = 0;
        if (texLoc < 0) texLoc = 1;
    #endif

    // Position
    if (posLoc >= 0) {
        glEnableVertexAttribArray((GLuint)posLoc);
        glVertexAttribPointer((GLuint)posLoc, 3, GL_FLOAT, GL_FALSE,
                              sizeof(TextVertex), (void*)0);
    }

    // TexCoord
    if (texLoc >= 0) {
        glEnableVertexAttribArray((GLuint)texLoc);
        glVertexAttribPointer((GLuint)texLoc, 2, GL_FLOAT, GL_FALSE,
                              sizeof(TextVertex), (void*)offsetof(TextVertex, texCoord));
    }
    
    glBindVertexArray(0);
}

void TextComponent::cleanupBuffers() {
#ifdef ENABLE_VULKAN
    vao = 0;
    vbo = 0;
    ebo = 0;
    return;
#endif
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (ebo) {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
}

void TextComponent::renderWorldSpace(IRenderer& renderer) {
#ifdef ENABLE_VULKAN
    (void)renderer;
    return;
#endif
    if (!textShader || !fontAtlasTexture) return;
    
    // Get camera matrices
    auto camera = renderer.getActiveCamera();
    if (!camera) return;
    
    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 projectionMatrix = camera->getProjectionMatrix();
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    
    // Get model matrix from transform
    glm::mat4 modelMatrix = owner->getWorldMatrix();
    
    // Use text shader
    textShader->use();
    
    // Set uniforms
    textShader->setMat4("uViewProjectionMat", viewProjectionMatrix);
    textShader->setMat4("uModelMat", modelMatrix);
    textShader->setInt("uFontAtlasTexture", 0);
    textShader->setVec4("uColor", color);
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthMaskWasEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(std::min(indices.size(), size_t(6000)) / 6 * 6), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    if (!blendWasEnabled) glDisable(GL_BLEND);
    if (depthMaskWasEnabled) glDepthMask(GL_TRUE);

    textShader->unuse();
}

void TextComponent::renderWorldSpace(IRenderer& renderer, const glm::mat4& worldTransform) {
#ifdef ENABLE_VULKAN
    (void)renderer;
    (void)worldTransform;
    return;
#endif
    if (!textShader || !fontAtlasTexture) return;
    
    // Get camera matrices
    auto camera = renderer.getActiveCamera();
    if (!camera) return;
    
    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 projectionMatrix = camera->getProjectionMatrix();
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    
    // Use the provided world transform
    glm::mat4 modelMatrix = worldTransform;
    
    // Use text shader
    textShader->use();
    
    // Set uniforms
    textShader->setMat4("uViewProjectionMat", viewProjectionMatrix);
    textShader->setMat4("uModelMat", modelMatrix);
    textShader->setInt("uFontAtlasTexture", 0);
    textShader->setVec4("uColor", color);
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthMaskWasEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(std::min(indices.size(), size_t(6000)) / 6 * 6), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    if (!blendWasEnabled) glDisable(GL_BLEND);
    if (depthMaskWasEnabled) glDepthMask(GL_TRUE);

    textShader->unuse();
}

void TextComponent::renderScreenSpace(IRenderer& renderer) {
#ifdef ENABLE_VULKAN
    (void)renderer;
    return;
#endif
    if (!textShader || !fontAtlasTexture) return;
    
    // Get camera matrices
    auto camera = renderer.getActiveCamera();
    if (!camera) return;
    
    // For screen space, use orthographic projection that covers the screen
    float aspectRatio = camera->getAspectRatio();
    float orthoHeight = 2.0f; // Smaller height for better text scaling
    float orthoWidth = orthoHeight * aspectRatio;
    
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::ortho(-orthoWidth/2, orthoWidth/2, -orthoHeight/2, orthoHeight/2, -1.0f, 1.0f);
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    
    // For screen space, position relative to screen coordinates, not world position
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    // Use only the X and Y components of the position for screen space positioning
    glm::vec3 screenPos = owner->getTransform().getPosition();
    // Convert world coordinates to screen coordinates (scale down for screen space)
    float screenX = screenPos.x * 0.1f; // Scale down X coordinate
    float screenY = screenPos.y * 0.1f; // Scale down Y coordinate
    modelMatrix = glm::translate(modelMatrix, glm::vec3(screenX, screenY, 0.0f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
    
    // Use text shader
    textShader->use();
    
    // Set uniforms
    textShader->setMat4("uViewProjectionMat", viewProjectionMatrix);
    textShader->setMat4("uModelMat", modelMatrix);
    textShader->setInt("uFontAtlasTexture", 0);
    textShader->setVec4("uColor", color);
    
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthMaskWasEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(std::min(indices.size(), size_t(6000)) / 6 * 6), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    if (!blendWasEnabled) glDisable(GL_BLEND);
    if (depthMaskWasEnabled) glDepthMask(GL_TRUE);
    textShader->unuse();
}

void TextComponent::renderWorldSpaceDirectly(const glm::mat4& worldTransform, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!textShader || !fontAtlasTexture) return;
    
    // Use text shader
    textShader->use();
    
    // Set uniforms
    textShader->setMat4("uViewProjectionMat", projectionMatrix * viewMatrix);
    textShader->setMat4("uModelMat", worldTransform);
    textShader->setInt("uFontAtlasTexture", 0);
    textShader->setVec4("uColor", color);
    
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthMaskWasEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(std::min(indices.size(), size_t(6000)) / 6 * 6), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    if (!blendWasEnabled) glDisable(GL_BLEND);
    if (depthMaskWasEnabled) glDepthMask(GL_TRUE);
    textShader->unuse();
}

void TextComponent::renderScreenSpaceDirectly() {
    if (!textShader || !fontAtlasTexture) return;
    
    // For screen space, use orthographic projection that covers the screen
    float orthoHeight = 2.0f; // Smaller height for better text scaling
    float orthoWidth = orthoHeight * (960.0f / 544.0f); // Use Vita's actual aspect ratio (960x544)
    
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::ortho(-orthoWidth/2, orthoWidth/2, -orthoHeight/2, orthoHeight/2, -1.0f, 1.0f);
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    
    // For screen space, position relative to screen coordinates
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    // Use only the X and Y components of the position for screen space positioning
    glm::vec3 screenPos = owner->getTransform().getPosition();
    // Convert world coordinates to screen coordinates (scale down for screen space)
    float screenX = screenPos.x * 0.1f; // Scale down X coordinate
    float screenY = screenPos.y * 0.1f; // Scale down Y coordinate
    modelMatrix = glm::translate(modelMatrix, glm::vec3(screenX, screenY, 0.0f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
    
    // Use text shader
    textShader->use();
    
    // Set uniforms
    textShader->setMat4("uViewProjectionMat", viewProjectionMatrix);
    textShader->setMat4("uModelMat", modelMatrix);
    textShader->setInt("uFontAtlasTexture", 0);
    textShader->setVec4("uColor", color);
    
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean depthMaskWasEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(std::min(indices.size(), size_t(6000)) / 6 * 6), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glDepthMask(depthMaskWasEnabled ? GL_TRUE : GL_FALSE);
    if (depthTestWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    if (!blendWasEnabled) glDisable(GL_BLEND);
    textShader->unuse();
}

glm::vec2 TextComponent::calculateTextSize() const {
    if (packedChars.empty()) return glm::vec2(0.0f);
    
    float pixelScale = 0.01f; // Same as in generateVertices
    float maxWidth = 0.0f;
    float currentWidth = 0.0f;
    int lineCount = 1;
    
    for (char ch : text) {
        if (ch == '\n') {
            maxWidth = std::max(maxWidth, currentWidth);
            currentWidth = 0.0f;
            lineCount++;
        } else if (static_cast<uint32_t>(ch) >= firstCharCodePoint && static_cast<uint32_t>(ch) < firstCharCodePoint + charsToInclude) {
            int charIndex = static_cast<int>(ch) - static_cast<int>(firstCharCodePoint);
            if (charIndex >= 0 && static_cast<size_t>(charIndex) < packedChars.size()) {
                currentWidth += packedChars[charIndex].xadvance * pixelScale * scale;
            }
        }
    }
    
    maxWidth = std::max(maxWidth, currentWidth);
    float totalHeight = fontSize * pixelScale * scale * lineCount * lineSpacing;
    
    return glm::vec2(maxWidth, totalHeight);
}
void TextComponent::generateVertices(TextRenderMode mode)
{
    vertices.clear();
    indices.clear();

    if (packedChars.empty() || text.empty())
        return;

    float pixelScale = 0.01f;

    glm::vec3 basePosition(0.0f);

    glm::vec2 textSize = calculateTextSize();
    float offsetX = 0.0f;

    switch (alignment)
    {
        case TextAlignment::CENTER:
            offsetX = -textSize.x * 0.5f;
            break;
        case TextAlignment::RIGHT:
            offsetX = -textSize.x;
            break;
        case TextAlignment::LEFT:
        default:
            offsetX = 0.0f;
            break;
    }

    glm::vec3 startPosition = basePosition + glm::vec3(offsetX, 0.0f, 0.0f);
    glm::vec3 currentPosition = startPosition;

    int order[6] = { 0, 1, 2, 0, 2, 3 };

    for (char ch : text)
    {
        if (ch == '\n')
        {
            currentPosition.x = startPosition.x;
            // The quad Y is flipped below (y = -q.y), so the pen advances in the
            // positive direction here to move each new line *down* on screen.
            currentPosition.y += fontSize * pixelScale * scale * lineSpacing;
            continue;
        }

        uint32_t codepoint = static_cast<uint32_t>(ch);

        if (codepoint < firstCharCodePoint ||
            codepoint >= firstCharCodePoint + charsToInclude)
            continue;

        int charIndex = static_cast<int>(codepoint - firstCharCodePoint);

        if (charIndex < 0 ||
            static_cast<size_t>(charIndex) >= packedChars.size())
            continue;

        float xpos = currentPosition.x / (pixelScale * scale);
        float ypos = currentPosition.y / (pixelScale * scale);

        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(
            packedChars.data(),
            atlasWidth,
            atlasHeight,
            charIndex,
            &xpos,
            &ypos,
            &q,
            0
        );

        float x0 = q.x0 * pixelScale * scale;
        float x1 = q.x1 * pixelScale * scale;
        float y0 = -q.y0 * pixelScale * scale;
        float y1 = -q.y1 * pixelScale * scale;
        glm::vec2 glyphVertices[4] =
        {
            { x1, y0 },
            { x0, y0 },
            { x0, y1 },
            { x1, y1 },
        };

        glm::vec2 glyphUVs[4] =
        {
            { q.s1, q.t0 },
            { q.s0, q.t0 },
            { q.s0, q.t1 },
            { q.s1, q.t1 },
        };

        size_t startIndex = vertices.size();

        for (int i = 0; i < 6; i++)
        {
            vertices.emplace_back(
                glm::vec3(glyphVertices[order[i]], currentPosition.z),
                glyphUVs[order[i]]
            );
        }

        for (int i = 0; i < 6; i++)
        {
            indices.push_back(static_cast<unsigned int>(startIndex + i));
        }

        currentPosition.x = xpos * pixelScale * scale;
    }
}

void TextComponent::updateBuffers() {
    if (vertices.empty()) return;

    // setupBuffers() allocates fixed-size buffers (1000 verts / 6000 indices)
    // clamp long text instead of overflowing them (GL_INVALID_VALUE + stale
    // geometry past 166 glyphs). Indices are clamped to whole quads
    constexpr size_t kMaxVertices = 1000;
    constexpr size_t kMaxIndices = 6000;
    const size_t vertexCount = std::min(vertices.size(), kMaxVertices);
    size_t indexCount = std::min(indices.size(), kMaxIndices);
    indexCount -= indexCount % 6;

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(TextVertex) * vertexCount, vertices.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(unsigned int) * indexCount, indices.data());

    glBindVertexArray(0);
}

void TextComponent::rebuildMesh()
{
    if (!textMesh)
        textMesh = std::make_shared<Mesh>();

    std::vector<Vertex> meshVertices;
    
    for (const auto& tv : vertices)
    {
        Vertex v{};

        v.position = tv.position;
        v.texCoords = tv.texCoord;

        meshVertices.push_back(v);
    }

    textMesh->setVertices(meshVertices);
    textMesh->setIndices(indices);
}

void TextComponent::cleanupFontAtlas() {
    fontAtlasTexture.reset();
    packedChars.clear();
    alignedQuads.clear();
}

} // namespace GameEngine
