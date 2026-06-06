#include <memory>
#include <glm/glm.hpp>

namespace GameEngine {

    class Shader;
    class Texture;

class TextMaterial {
public:

    glm::vec4 getColor() const { return color; }
    std::shared_ptr<FontAtlas> getFontAtlas() const {return fontAtlas;}

    std::shared_ptr<Shader> shader;
    std::shared_ptr<FontAtlas> fontAtlas;

    glm::vec4 color{1.0f};

    //BlendMode blendMode = BlendMode::Alpha;
    bool depthWrite = false;
};
}