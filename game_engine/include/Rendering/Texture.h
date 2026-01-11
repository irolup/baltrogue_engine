#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <memory>
#include "Platform.h"

namespace GameEngine {

enum class TextureFilter {
    NEAREST,
    LINEAR,
    NEAREST_MIPMAP_NEAREST,
    LINEAR_MIPMAP_NEAREST,
    NEAREST_MIPMAP_LINEAR,
    LINEAR_MIPMAP_LINEAR
};

enum class TextureWrap {
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER
};

enum class TextureFormat {
    RGB,
    RGBA,
    DEPTH_COMPONENT,
    DEPTH_STENCIL
};

class Texture {
public:
    Texture();
    ~Texture();
    
    bool loadFromFile(const std::string& filepath);
    bool createEmpty(int width, int height, TextureFormat format = TextureFormat::RGBA);
    bool createFromData(const void* data, int width, int height, TextureFormat format = TextureFormat::RGBA);
    
    bool loadSTBImage(const std::string& filepath);
    
    void bind(int textureUnit = 0) const;
    void unbind() const;
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    GLuint getID() const { return textureID; }
    void setTextureID(GLuint id) { textureID = id; }
    TextureFormat getFormat() const { return format; }
    const std::string& getFilePath() const { return filepath; }
    
    void setFilter(TextureFilter minFilter, TextureFilter magFilter);
    void setWrap(TextureWrap wrapS, TextureWrap wrapT);
    void generateMipmaps();
    
    static std::shared_ptr<Texture> getWhiteTexture();
    static std::shared_ptr<Texture> getBlackTexture();
    static std::shared_ptr<Texture> getErrorTexture();
    
private:
    GLuint textureID;
    int width, height;
    TextureFormat format;
    std::string filepath;
    
    GLenum getGLFormat(TextureFormat fmt) const;
    GLenum getGLFilter(TextureFilter filter) const;
    GLenum getGLWrap(TextureWrap wrap) const;
};

} // namespace GameEngine

#endif // TEXTURE_H
