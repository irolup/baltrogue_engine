#include "Components/TextComponent.h"
#include "Rendering/Renderer.h"
#include "Rendering/FontManager.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"
#include "Components/CameraComponent.h"
#include "Scene/SceneNode.h"
#include <iostream>
#include <algorithm>
#include <map>

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

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
    // TextComponent::start called
    if (isInitialized) {
        // Already initialized, don't initialize again
        return;
    }
    
    initializeFont();
    setupBuffers();
    isInitialized = true;
    
    // Generate initial mesh data
    if (needsUpdate) {
        updateTextMesh();
        needsUpdate = false;
    }
    
    // TextComponent::start completed
}

void TextComponent::update(float deltaTime) {
    if (needsUpdate && isInitialized) {
        updateTextMesh();
        needsUpdate = false;
    }
}

void TextComponent::render(Renderer& renderer) {
    if (!isInitialized || text.empty() || !fontAtlasTexture) {
        return;
    }
    
    if (renderMode == TextRenderMode::WORLD_SPACE) {
        renderWorldSpace(renderer);
    } else {
        renderScreenSpace(renderer);
    }
}

void TextComponent::render(Renderer& renderer, const glm::mat4& worldTransform) {
    // TextComponent::render called
    
    if (!isInitialized || text.empty() || !fontAtlasTexture) {
        // Early return due to missing data
        return;
    }
    
    if (renderMode == TextRenderMode::WORLD_SPACE) {
        // Rendering world space text
        renderWorldSpace(renderer, worldTransform);
    } else {
        // Rendering screen space text
        renderScreenSpace(renderer);
    }
}

void TextComponent::destroy() {
    cleanupBuffers();
    cleanupFontAtlas();
}

void TextComponent::setText(const std::string& newText) {
    if (text != newText) {
        text = newText;
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
        fontAtlasTexture = atlas->texture;
        packedChars = atlas->packedChars;
        alignedQuads = atlas->alignedQuads;
        
        // Create text shader
        textShader = std::make_shared<Shader>();
        #ifdef VITA_BUILD
        // Vita CG shaders need matrices transposed
            textShader->needsTranspose = true;
        #else
            textShader->needsTranspose = false; // Linux or other platforms
        #endif
        
#ifdef LINUX_BUILD
        // Try to load external shaders first
        if (!textShader->loadFromFiles("assets/linux_shaders/text.vert", "assets/linux_shaders/text.frag")) {
            // Fallback to embedded shaders
            std::string vertexSource = R"(
                #version 120
                attribute vec3 aPosition;
                attribute vec4 aColor;
                attribute vec2 aTexCoord;
                
                uniform mat4 uViewProjectionMat;
                uniform mat4 uModelMat;
                
                varying vec4 color;
                varying vec2 texCoord;
                
                void main()
                {
                    gl_Position = uViewProjectionMat * uModelMat * vec4(aPosition, 1.0);
                    color = aColor;
                    texCoord = aTexCoord;
                }
            )";
            
            std::string fragmentSource = R"(
                #version 120
                varying vec4 color;
                varying vec2 texCoord;
                
                uniform sampler2D uFontAtlasTexture;
                
                void main()
                {
                    float alpha = texture2D(uFontAtlasTexture, texCoord).r;
                    gl_FragColor = vec4(color.rgb, color.a * alpha);
                }
            )";
            
            textShader->loadFromSource(vertexSource, fragmentSource);
        }
#else
        // Vita builds - try to load external shaders first
        if (!textShader->loadFromFiles("app0:/text.vert", "app0:/text.frag")) {
            // Fallback to embedded CG/HLSL shaders (VitaGL uses CG/HLSL)
            std::string vertexSource = R"(
                struct VS_INPUT {
                    float3 aPosition : POSITION;
                    float4 aColor : COLOR;
                    float2 aTexCoord : TEXCOORD0;
                };
                
                struct VS_OUTPUT {
                    float4 Position : POSITION;
                    float4 color : COLOR;
                    float2 texCoord : TEXCOORD0;
                };
                
                float4x4 uViewProjectionMat;
                float4x4 uModelMat;
                
                VS_OUTPUT main(VS_INPUT input) {
                    VS_OUTPUT output;
                    output.Position = mul(uViewProjectionMat, mul(uModelMat, float4(input.aPosition, 1.0)));
                    output.color = input.aColor;
                    output.texCoord = input.aTexCoord;
                    return output;
                }
            )";
            
            std::string fragmentSource = R"(
                struct PS_INPUT {
                    float4 color : COLOR;
                    float2 texCoord : TEXCOORD0;
                };
                
                sampler2D uFontAtlasTexture;
                
                float4 main(PS_INPUT input) : COLOR {
                    float alpha = tex2D(uFontAtlasTexture, input.texCoord).r;
                    return float4(input.color.rgb, input.color.a * alpha);
                }
            )";
            
            if (!textShader->loadFromSource(vertexSource, fragmentSource)) {
                std::cerr << "Failed to load embedded CG/HLSL text shaders!" << std::endl;
                textShader.reset();
            }
        }
#endif
        
        if (!textShader || !textShader->isValid()) {
            std::cerr << "Failed to create text shader!" << std::endl;
            textShader.reset();
        }
    } else {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
    }
}

void TextComponent::updateTextMesh() {
    generateVertices();
    updateBuffers();
}

void TextComponent::setupBuffers() {
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
        GLint colorLoc = glGetAttribLocation(textShader->getProgram(), "aColor");
        GLint texLoc   = glGetAttribLocation(textShader->getProgram(), "aTexCoord");
    #else
        GLint posLoc   = 0; // POSITION
        GLint colorLoc = 1; // COLOR
        GLint texLoc   = 2; // TEXCOORD0
    #endif

    // Position
    if (posLoc >= 0) {
        glEnableVertexAttribArray((GLuint)posLoc);
        glVertexAttribPointer((GLuint)posLoc, 3, GL_FLOAT, GL_FALSE,
                              sizeof(TextVertex), (void*)0);
    }

    // Color
    if (colorLoc >= 0) {
        glEnableVertexAttribArray((GLuint)colorLoc);
        glVertexAttribPointer((GLuint)colorLoc, 4, GL_FLOAT, GL_FALSE,
                              sizeof(TextVertex), (void*)offsetof(TextVertex, color));
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

void TextComponent::renderWorldSpace(Renderer& renderer) {
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
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    // Store current blending state
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore blending state
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    
    textShader->unuse();
}

void TextComponent::renderWorldSpace(Renderer& renderer, const glm::mat4& worldTransform) {
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
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    // Store current blending state
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore blending state
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    
    textShader->unuse();
}

void TextComponent::renderScreenSpace(Renderer& renderer) {
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
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    // Store current blending state
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore blending state
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    
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
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    // Store current blending state
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore blending state
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    
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
    
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    fontAtlasTexture->bind();
    
    // Store current blending state
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore blending state
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    
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

void TextComponent::generateVertices() {
    vertices.clear();
    indices.clear();
    
    if (packedChars.empty() || text.empty()) return;
    
    // Text processing (debug output removed for performance)
    
    // Calculate pixel scale to convert from font atlas pixels to world units
    // Use a smaller scale factor to make text readable
    float pixelScale = 0.01f; // Scale down significantly
    glm::vec3 position = glm::vec3(0.0f);
    
    // Calculate text bounds for alignment
    glm::vec2 textSize = calculateTextSize();
    float offsetX = 0.0f;
    
    switch (alignment) {
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
    
    glm::vec3 startPosition = position + glm::vec3(offsetX, textSize.y * 0.5f, 0.0f);
    glm::vec3 currentPosition = startPosition;
    
    int order[6] = { 0, 1, 2, 0, 2, 3 };
    
    for (char ch : text) {
        if (ch == '\n') {
            currentPosition.x = startPosition.x;
            currentPosition.y -= fontSize * pixelScale * scale * lineSpacing;
        } else if (static_cast<uint32_t>(ch) >= firstCharCodePoint && static_cast<uint32_t>(ch) < firstCharCodePoint + charsToInclude) {
            int charIndex = static_cast<int>(ch) - static_cast<int>(firstCharCodePoint);
            // Character processing (debug output removed for performance)
            if (charIndex >= 0 && static_cast<size_t>(charIndex) < packedChars.size() && static_cast<size_t>(charIndex) < alignedQuads.size()) {
                stbtt_packedchar* packedChar = &packedChars[charIndex];
                stbtt_aligned_quad* alignedQuad = &alignedQuads[charIndex];
                
                // Calculate glyph size and position
                glm::vec2 glyphSize = {
                    (packedChar->x1 - packedChar->x0) * pixelScale * scale,
                    (packedChar->y1 - packedChar->y0) * pixelScale * scale
                };
                
                glm::vec2 glyphBoundingBoxBottomLeft = {
                    currentPosition.x + (packedChar->xoff * pixelScale * scale),
                    currentPosition.y + (packedChar->yoff * pixelScale * scale)
                };
                
                // Create quad vertices
                glm::vec2 glyphVertices[4] = {
                    { glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y + glyphSize.y },
                    { glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y + glyphSize.y },
                    { glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y },
                    { glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y },
                };
                
                glm::vec2 glyphTextureCoords[4] = {
                    { alignedQuad->s1, alignedQuad->t0 },
                    { alignedQuad->s0, alignedQuad->t0 },
                    { alignedQuad->s0, alignedQuad->t1 },
                    { alignedQuad->s1, alignedQuad->t1 },
                };
                
                // Add vertices for this character
                size_t startIndex = vertices.size();
                for (int i = 0; i < 6; i++) {
                    vertices.emplace_back(
                        glm::vec3(glyphVertices[order[i]], currentPosition.z),
                        color,
                        glyphTextureCoords[order[i]]
                    );
                }
                
                // Add indices
                for (int i = 0; i < 6; i++) {
                    indices.push_back(static_cast<unsigned int>(startIndex + i));
                }
                
                // Advance position
                currentPosition.x += packedChar->xadvance * pixelScale * scale;
            }
        }
    }
}

void TextComponent::updateBuffers() {
    if (vertices.empty()) return;
    
    glBindVertexArray(vao);
    
    // Update vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(TextVertex) * vertices.size(), vertices.data());
    
    // Update element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(unsigned int) * indices.size(), indices.data());
    
    glBindVertexArray(0);
}

void TextComponent::cleanupFontAtlas() {
    fontAtlasTexture.reset();
    packedChars.clear();
    alignedQuads.clear();
}

} // namespace GameEngine
