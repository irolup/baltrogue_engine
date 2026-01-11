#ifndef MESH_RENDERER_H
#define MESH_RENDERER_H

#include "Components/Component.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include <memory>

namespace GameEngine {

class MeshRenderer : public Component {
public:
    MeshRenderer();
    virtual ~MeshRenderer();
    
    COMPONENT_TYPE(MeshRenderer)
    
    virtual void render(Renderer& renderer) override;
    
    void setMesh(std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> getMesh() const { return mesh; }
    
    void setMaterial(std::shared_ptr<Material> material);
    std::shared_ptr<Material> getMaterial() const { return material; }
    
    bool getCastShadows() const { return castShadows; }
    void setCastShadows(bool cast) { castShadows = cast; }
    
    bool getReceiveShadows() const { return receiveShadows; }
    void setReceiveShadows(bool receive) { receiveShadows = receive; }
    
    virtual void drawInspector() override;
    
private:
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool castShadows;
    bool receiveShadows;
};

} // namespace GameEngine

#endif // MESH_RENDERER_H
