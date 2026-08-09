#include "Rendering/ShadowAtlasGL.h"

#include <iostream>

namespace GameEngine {

ShadowAtlasGL::~ShadowAtlasGL() {
    destroy();
}

void ShadowAtlasGL::destroy() {
#ifndef ENABLE_VULKAN
    if (texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (depthRenderbuffer) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
    if (framebuffer) {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }
#endif
    width = 0;
    height = 0;
}

bool ShadowAtlasGL::ensureCreated(int requestedWidth, int requestedHeight) {
#ifdef ENABLE_VULKAN
    (void)requestedWidth;
    (void)requestedHeight;
    return false;
#else
    if (requestedWidth <= 0 || requestedHeight <= 0) {
        return false;
    }

    if (framebuffer != 0 && width == requestedWidth && height == requestedHeight) {
        return !creationFailed;
    }

    // A failed attempt at one size should not stop a later, smaller one.
    destroy();
    creationFailed = false;

    width = requestedWidth;
    height = requestedHeight;

    GLint previousFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

#ifdef VITA_BUILD
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    // Sampled as a plain texture: the lit shaders compare depths themselves
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif

    // Packed depth (Vita) cannot be blended between texels and a comparison
    // against a filtered depth (desktop) is meaningless, so both stay on
    // GL_NEAREST and the shaders do their own filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef VITA_BUILD
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
#else
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
#endif

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ShadowAtlasGL: framebuffer incomplete (status 0x" << std::hex << status << std::dec
                  << "), shadows disabled" << std::endl;
        destroy();
        creationFailed = true;
        return false;
    }

    return true;
#endif
}

void ShadowAtlasGL::bindForWriting() const {
#ifndef ENABLE_VULKAN
    if (!isValid()) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
#endif
}

void ShadowAtlasGL::bindTexture(int textureUnit) const {
#ifndef ENABLE_VULKAN
    if (!isValid()) {
        return;
    }
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, texture);
#else
    (void)textureUnit;
#endif
}

}
