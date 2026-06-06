#pragma once

#include "Rendering/OpenGLRenderer.h"

namespace GameEngine {

// Real class (not a typedef) so forward declarations and overrides stay valid.
class Renderer : public OpenGLRenderer {
public:
    using OpenGLRenderer::OpenGLRenderer;
};

} // namespace GameEngine