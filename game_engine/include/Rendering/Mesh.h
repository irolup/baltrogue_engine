#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <memory>
#include "Platform.h"

namespace GameEngine {

class Material; // Forward declaration

enum class MeshType {
    UNKNOWN,
    QUAD,
    PLANE,
    CUBE,
    SPHERE,
    CAPSULE,
    CYLINDER,
    CUSTOM
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    
    Vertex() : position(0.0f), normal(0.0f, 1.0f, 0.0f), texCoords(0.0f), tangent(1.0f, 0.0f, 0.0f) {}
    Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& tex)
        : position(pos), normal(norm), texCoords(tex), tangent(1.0f, 0.0f, 0.0f) {}
};

class Mesh {
public:
    Mesh();
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();
    
    void setVertices(const std::vector<Vertex>& vertices);
    void setIndices(const std::vector<unsigned int>& indices);
    void upload();
    
    void bind() const;
    void unbind() const;
    void draw() const;
    void draw(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
    void draw(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const class Material& material) const;
    void drawDirectCube(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& color) const;
    void drawDirectCube(const glm::vec3& color) const;
    
    size_t getVertexCount() const { return vertices.size(); }
    size_t getIndexCount() const { return indices.size(); }
    size_t getTriangleCount() const { return indices.size() / 3; }
    MeshType getMeshType() const { return meshType; }
    void setMeshType(MeshType type) { meshType = type; }
    
    void setRenderMode(GLenum mode) { renderMode = mode; }
    GLenum getRenderMode() const { return renderMode; }
    
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<unsigned int>& getIndices() const { return indices; }
    
    glm::vec3 getBoundsMin() const { return boundsMin; }
    glm::vec3 getBoundsMax() const { return boundsMax; }
    glm::vec3 getBoundsCenter() const { return (boundsMin + boundsMax) * 0.5f; }
    glm::vec3 getBoundsSize() const { return boundsMax - boundsMin; }
    
    static std::shared_ptr<Mesh> createQuad();
    static std::shared_ptr<Mesh> createPlane(float width = 1.0f, float height = 1.0f, int subdivisions = 1);    
    static std::shared_ptr<Mesh> createCube();
    static std::shared_ptr<Mesh> createSphere(int segments, int rings, float radius = 0.5f);
    static std::shared_ptr<Mesh> createCapsule(float radius, float halfHeight, int segments = 16, int rings = 8);
    static std::shared_ptr<Mesh> createCylinder(float radius, float halfHeight, int segments = 16);

    static std::shared_ptr<Mesh> createWireframeBox(const glm::vec3& halfExtents = glm::vec3(0.5f));
    static std::shared_ptr<Mesh> createWireframeSphere(float radius = 0.5f, int segments = 16);
    static std::shared_ptr<Mesh> createWireframeCapsule(float radius = 0.5f, float height = 1.0f, int segments = 16);
    static std::shared_ptr<Mesh> createWireframeCylinder(float radius = 0.5f, float height = 1.0f, int segments = 16);
    static std::shared_ptr<Mesh> createWireframePlane(float width = 1.0f, float height = 1.0f);

    static std::shared_ptr<Mesh> loadFromFile(const std::string& filepath);
    
private:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    GLuint VAO, VBO, EBO;
    bool uploaded;
    
    glm::vec3 boundsMin, boundsMax;
    MeshType meshType;
    
    GLenum renderMode;
    
    void calculateBounds();
    void calculateTangents();
    void setupBuffers();
    void cleanupBuffers();
};

} // namespace GameEngine

#endif // MESH_H
