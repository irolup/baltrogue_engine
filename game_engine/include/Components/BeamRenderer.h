#ifndef BEAM_RENDERER_H
#define BEAM_RENDERER_H

#include "Components/Component.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include <memory>
#include <glm/glm.hpp>

namespace GameEngine {

class BeamRenderer : public Component {
public:
    BeamRenderer();
    virtual ~BeamRenderer();

    COMPONENT_TYPE(BeamRenderer)

    virtual void render(Renderer& renderer) override;

    void setMaterial(std::shared_ptr<Material> mat) { material = mat; }
    std::shared_ptr<Material> getMaterial() const { return material; }
    std::shared_ptr<Mesh> getBeamMesh() const { return beamMesh; }

    void setBeamStart(const glm::vec3& worldPos) { beamStart = worldPos; }
    void setBeamEnd(const glm::vec3& worldPos) { beamEnd = worldPos; }
    void setBeamWidth(float width) { beamHalfWidth = width * 0.5f; }
    glm::vec3 getBeamStart() const { return beamStart; }
    glm::vec3 getBeamEnd() const { return beamEnd; }
    float getBeamWidth() const { return beamHalfWidth * 2.0f; }

    virtual void drawInspector() override;

private:
    std::shared_ptr<Mesh> beamMesh;
    std::shared_ptr<Material> material;
    glm::vec3 beamStart;
    glm::vec3 beamEnd;
    float beamHalfWidth;
};

} // namespace GameEngine

#endif // BEAM_RENDERER_H
