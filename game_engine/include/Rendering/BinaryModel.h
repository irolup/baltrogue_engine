#ifndef BINARY_MODEL_H
#define BINARY_MODEL_H

#include <vector>
#include <string>
#include <memory>

namespace GameEngine {
    class Mesh;
    class Material;
}

namespace GameEngine {

struct BinaryModelHeader {
    char magic[4];
    uint32_t version;
    uint32_t meshCount;
    uint32_t materialCount;
    uint32_t totalSize;
    char reserved[16];
};

struct BinaryMeshHeader {
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexSize;
    uint32_t indexSize;
    uint32_t materialIndex;
    char name[32];
};

struct BinaryMaterialHeader {
    float color[4];
    uint32_t diffuseTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t armTextureIndex;
    char name[32];
};

struct BinaryTextureInfo {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t dataSize;
    char path[128];
};

struct BinaryModelData {
    BinaryModelHeader header;
    std::vector<BinaryMeshHeader> meshHeaders;
    std::vector<BinaryMaterialHeader> materialHeaders;
    std::vector<BinaryTextureInfo> textureInfos;
    std::vector<std::vector<float>> vertexData;
    std::vector<std::vector<uint16_t>> indexData;
    std::vector<std::vector<uint8_t>> textureData;
};

class BinaryModel {
public:
    BinaryModel();
    ~BinaryModel();
    
    bool loadFromGLTF(const std::string& gltfPath);
    
    bool saveToBinary(const std::string& binaryPath);
    
    bool loadFromBinary(const std::string& binaryPath);
    
    std::vector<std::shared_ptr<Mesh>> createMeshes();
    std::vector<std::shared_ptr<Material>> createMaterials();
    
    const BinaryModelData& getData() const { return data; }
    bool isLoaded() const { return loaded; }
    
private:
    BinaryModelData data;
    bool loaded;
    
    bool validateHeader(const BinaryModelHeader& header);
    void clearData();
    
    bool convertGLTFToBinary(const std::string& gltfPath);
    void optimizeForVita();
    
    bool writeHeader(std::ofstream& file);
    bool writeMeshData(std::ofstream& file);
    bool writeMaterialData(std::ofstream& file);
    bool writeTextureData(std::ofstream& file);
    
    bool readHeader(std::ifstream& file);
    bool readMeshData(std::ifstream& file);
    bool readMaterialData(std::ifstream& file);
    bool readTextureData(std::ifstream& file);
};

} // namespace GameEngine

#endif // BINARY_MODEL_H
