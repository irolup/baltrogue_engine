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
}

#include "../../vendor/stb/stb_truetype.h"

namespace GameEngine {

struct TextVertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 texCoord;
    
    TextVertex() : position(0.0f), color(1.0f), texCoord(0.0f) {}
    TextVertex(const glm::vec3& pos, const glm::vec4& col, const glm::vec2& tex)
        : position(pos), color(col), texCoord(tex) {}
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
    virtual void render(Renderer& renderer) override;
    void render(Renderer& renderer, const glm::mat4& worldTransform);
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
    uint32_t atlasWidth;
    uint32_t atlasHeight;
    uint32_t charsToInclude;
    uint32_t firstCharCodePoint;
    
    std::vector<TextVertex> vertices;
    std::vector<unsigned int> indices;
    std::shared_ptr<Shader> textShader;
    
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    
    bool needsUpdate;
    bool isInitialized;
    
    void initializeFont();
    void updateTextMesh();
    void setupBuffers();
    void cleanupBuffers();
    void renderWorldSpace(Renderer& renderer);
    void renderWorldSpace(Renderer& renderer, const glm::mat4& worldTransform);
    void renderScreenSpace(Renderer& renderer);
    
    glm::vec2 calculateTextSize() const;
    void generateVertices();
    void updateBuffers();
    
    bool generateFontAtlas();
    void cleanupFontAtlas();
};

} // namespace GameEngine

#endif // TEXT_COMPONENT_H
