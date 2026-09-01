#include <memory>
#include <glm/glm.hpp>

namespace GameEngine {

    class Shader;
    class Texture;

class TextMaterial {
public:

    glm::vec4 getColor() const { return color; }


    void setColor(const glm::vec4& newColor) {
        if (color != newColor) {
            color = newColor;
            revision = ++nextRevision;
        }
    }
    uint32_t getRevision() const { return revision; }

    std::shared_ptr<FontAtlas> getFontAtlas() const {return fontAtlas;}

    std::shared_ptr<Shader> shader;
    std::shared_ptr<FontAtlas> fontAtlas;

    //BlendMode blendMode = BlendMode::Alpha;
    bool depthWrite = false;

private:
    glm::vec4 color{1.0f};
    uint32_t revision = 0;
    static uint32_t nextRevision;
};
}