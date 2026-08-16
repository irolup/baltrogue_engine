#ifndef MODEL_RENDERER_H
#define MODEL_RENDERER_H

#include "Components/Component.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace tinygltf {
    class Model;
    class Node;
    class Mesh;
    class Primitive;
    struct Image;
}

namespace GameEngine {

struct ModelData {
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<glm::mat4> meshNodeTransforms;
    std::vector<int> meshMaterialIndices;
    std::string modelPath;
    std::string modelName;
    bool isLoaded;
    
    ModelData() : isLoaded(false) {}
};

class ModelRenderer : public Component {
public:
    ModelRenderer();
    virtual ~ModelRenderer();
    
    COMPONENT_TYPE(ModelRenderer)
    
    virtual void render(IRenderer& renderer) override;
    
    bool loadModel(const std::string& modelPath);
    void unloadModel();
    
    const std::string& getModelPath() const { return modelData.modelPath; }
    const std::string& getModelName() const { return modelData.modelName; }
    bool isModelLoaded() const { return modelData.isLoaded; }
    
    const std::vector<std::shared_ptr<Mesh>>& getMeshes() const { return modelData.meshes; }
    const std::vector<std::shared_ptr<Material>>& getMaterials() const { return modelData.materials; }
    const std::vector<int>& getMeshMaterialIndices() const { return modelData.meshMaterialIndices; }
    const std::vector<glm::mat4>& getMeshNodeTransforms() const { return modelData.meshNodeTransforms; }

    std::shared_ptr<Material> getRenderMaterial(size_t meshIndex);

    bool getCastShadows() const { return castShadows; }
    void setCastShadows(bool cast) { castShadows = cast; }
    
    bool getReceiveShadows() const { return receiveShadows; }
    void setReceiveShadows(bool receive) { receiveShadows = receive; }
    
    virtual void drawInspector() override;
    
    static std::vector<std::string> discoverModels(const std::string& directory = "assets/models");
    
private:
    ModelData modelData;
    bool castShadows;
    bool receiveShadows;
    
    bool loadGLTFModel(const std::string& modelPath);
    std::shared_ptr<Mesh> createMeshFromGLTF(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh, const tinygltf::Primitive& primitive);
    std::shared_ptr<Material> createMaterialFromGLTF(const tinygltf::Model& gltfModel, int materialIndex, const std::string& modelPath);
    
    static glm::mat4 computeNodeTransform(const tinygltf::Node& node);
    void traverseGLTFNodes(const tinygltf::Model& gltfModel, const tinygltf::Node& node, 
                          const glm::mat4& parentTransform);
    
    bool loadBinaryModel(const std::string& modelPath);
    bool saveBinaryModel(const std::string& modelPath);
    
    std::vector<glm::mat4> getBindPoseBoneTransforms() const;

    mutable std::shared_ptr<const std::vector<glm::mat4>> cachedBindPose_;
    mutable bool bindPoseCached_ = false;
    
    static std::string getFileExtension(const std::string& filepath);
    static std::string getFileName(const std::string& filepath);
    
    static std::unordered_map<std::string, std::shared_ptr<ModelData>> meshCache;
    static std::shared_ptr<ModelData> getCachedModel(const std::string& modelPath);

public:
    /** Drop the shared CPU-side model cache (all scenes must be unloaded). */
    static void clearMeshCache();

private:

#ifdef ENABLE_VULKAN
    struct DecodedGltfImage {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
        bool valid = false;
    };

    static DecodedGltfImage decodeGltfImage(const tinygltf::Image& image);
    static std::string resolveGltfImageFilePath(const std::string& modelPath, const tinygltf::Image& image, const std::string& defaultPath);
    static void assignVulkanGltfTexture(
        Material& material,
        const tinygltf::Image& image,
        const std::string& modelPath,
        int materialIndex,
        const char* slot,
        const std::string& defaultPath,
        void (Material::*setPathFn)(const std::string&));
#endif
};

} // namespace GameEngine

#endif // MODEL_RENDERER_H
