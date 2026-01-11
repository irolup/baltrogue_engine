#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Platform.h"
#include <memory>
#include <glm/glm.hpp>

namespace GameEngine {

class Texture;

class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();
    
    bool create(int width, int height);
    void destroy();
    
    void bind() const;
    void unbind() const;
    
    void resize(int width, int height);
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    GLuint getColorTexture() const { return colorTexture; }
    GLuint getDepthTexture() const { return depthTexture; }
    GLuint getFramebufferID() const { return framebufferID; }
    
    bool isValid() const;
    
    void clear(const glm::vec3& clearColor = glm::vec3(0.2f, 0.3f, 0.3f)) const;
    
private:
    GLuint framebufferID;
    GLuint colorTexture;
    GLuint depthTexture;
    int width, height;
    
    void createTextures();
    void attachTextures();
};

} // namespace GameEngine

#endif // FRAMEBUFFER_H
