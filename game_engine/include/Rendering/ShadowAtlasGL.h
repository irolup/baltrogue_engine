#ifndef SHADOW_ATLAS_GL_H
#define SHADOW_ATLAS_GL_H

#include "Platform.h"

namespace GameEngine {

//Render target the OpenGL and VitaGL backends fill during the shadow pass.
//Desktop GL renders straight into a depth texture with no colour attachment.
//VitaGL cannot attach a depth texture for sampling glFramebufferTexture2D
//only accepts GL_COLOR_ATTACHMENT0, so on the Vita the pass writes depth packed
//into an RGBA8 colour texture with a plain depth renderbuffer behind it. 


class ShadowAtlasGL {
public:
    ShadowAtlasGL() = default;
    ~ShadowAtlasGL();

    ShadowAtlasGL(const ShadowAtlasGL&) = delete;
    ShadowAtlasGL& operator=(const ShadowAtlasGL&) = delete;

    // Allocates on first use and reallocates when the size changes.
    bool ensureCreated(int width, int height);
    void destroy();

    void bindForWriting() const;
    void bindTexture(int textureUnit) const;

    bool isValid() const { return framebuffer != 0 && !creationFailed; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    GLuint framebuffer = 0;
    GLuint texture = 0;
    GLuint depthRenderbuffer = 0;
    int width = 0;
    int height = 0;
    bool creationFailed = false;
};

}

#endif
