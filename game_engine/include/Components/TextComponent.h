#ifndef TEXT_COMPONENT_H
#define TEXT_COMPONENT_H

#include "Components/Component.h"
#include "Platform.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {
    class Renderer;
    class Shader;
    class Texture;
    class FontManager;
    class SceneNode;
    class Mesh;
    class TextMaterial;
    class FontAtlas;
}

#include "../../vendor/stb/stb_truetype.h"

namespace GameEngine {

struct TextVertex {
    glm::vec3 position;
    glm::vec2 texCoord;

    TextVertex() : position(0.0f), texCoord(0.0f) {}
    TextVertex(const glm::vec3& pos, const glm::vec2& tex)
        : position(pos), texCoord(tex) {}
};

enum class TextAlignment {
    LEFT,
    CENTER,
    RIGHT
};

enum class TextRenderMode {
    WORLD_SPACE,    // Text rendered in 3D world space
    SCREEN_SPACE    // Text rendered in screen space (UI-like, follows camera)
};

class TextComponent : public Component {
public:
    TextComponent();
    virtual ~TextComponent();
    
    COMPONENT_TYPE(TextComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void render(IRenderer& renderer) override;
    void render(IRenderer& renderer, const glm::mat4& worldTransform);
    virtual void destroy() override;
    
    void renderWorldSpaceDirectly(const glm::mat4& worldTransform, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void renderScreenSpaceDirectly();
    
    void setText(const std::string& text);
    const std::string& getText() const { return text; }
    
    void setFontPath(const std::string& fontPath);
    const std::string& getFontPath() const { return fontPath; }
    
    void setFontSize(float size);
    float getFontSize() const { return fontSize; }
    
    void setColor(const glm::vec4& color);
    glm::vec4 getColor() const { return color; }
    
    void setAlignment(TextAlignment alignment);
    TextAlignment getAlignment() const { return alignment; }
    
    void setRenderMode(TextRenderMode mode);
    TextRenderMode getRenderMode() const { return renderMode; }
    
    void setScale(float scale);
    float getScale() const { return scale; }
    
    void setLineSpacing(float spacing);
    float getLineSpacing() const { return lineSpacing; }
    
    glm::vec2 getTextBounds() const;
    float getTextWidth() const;
    float getTextHeight() const;

    // Accessors for Vulkan text upload
    const std::vector<TextVertex>& getCpuTextVertices() const { return vertices; }
    const std::vector<unsigned int>& getCpuIndices() const { return indices; }

    virtual void drawInspector() override;
    
private:
    std::string text;
    std::string fontPath;
    float fontSize;
    glm::vec4 color;
    TextAlignment alignment;
    TextRenderMode renderMode;
    float scale;
    float lineSpacing;
    
    std::vector<stbtt_packedchar> packedChars;
    std::vector<stbtt_aligned_quad> alignedQuads;
    std::shared_ptr<Texture> fontAtlasTexture;
    std::shared_ptr<FontAtlas> fontAtlas;
    uint32_t atlasWidth;
    uint32_t atlasHeight;
    uint32_t charsToInclude;
    uint32_t firstCharCodePoint;
    
    std::vector<TextVertex> vertices;
    std::vector<unsigned int> indices;
    std::shared_ptr<Shader> textShader;
    std::shared_ptr<Mesh> textMesh;
    std::shared_ptr<TextMaterial> textMaterial;
    
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    
    bool needsUpdate;
    bool isInitialized;
    
    void initializeFont();
    void updateTextMesh();
    void setupBuffers();
    void cleanupBuffers();
    void renderWorldSpace(IRenderer& renderer);
    void renderWorldSpace(IRenderer& renderer, const glm::mat4& worldTransform);
    void renderScreenSpace(IRenderer& renderer);
    
    glm::vec2 calculateTextSize() const;
    void generateVertices(TextRenderMode mode);
    void updateBuffers();

#ifdef ENABLE_VULKAN
    glm::mat4 buildClipMatrix(IRenderer& renderer, const glm::mat4& worldTransform) const;
#endif
    
    void rebuildMesh();
    void cleanupFontAtlas();


};

} // namespace GameEngine

#endif // TEXT_COMPONENT_H
