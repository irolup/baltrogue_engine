#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include <iostream>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace GameEngine {

Mesh::Mesh()
    : VAO(0), VBO(0), EBO(0), uploaded(false)
    , boundsMin(std::numeric_limits<float>::max())
    , boundsMax(std::numeric_limits<float>::lowest())
    , meshType(MeshType::UNKNOWN)
    , renderMode(GL_TRIANGLES)
{
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : vertices(vertices), indices(indices)
    , VAO(0), VBO(0), EBO(0), uploaded(false)
    , boundsMin(std::numeric_limits<float>::max())
    , boundsMax(std::numeric_limits<float>::lowest())
    , meshType(MeshType::UNKNOWN)
    , renderMode(GL_TRIANGLES)
{
    calculateBounds();
}

Mesh::~Mesh() {
    cleanupBuffers();
}

void Mesh::setVertices(const std::vector<Vertex>& newVertices) {
    vertices = newVertices;
    calculateBounds();
    uploaded = false;
}

void Mesh::setIndices(const std::vector<unsigned int>& newIndices) {
    indices = newIndices;
    uploaded = false;
}

void Mesh::upload() {
    if (uploaded) {
        cleanupBuffers();
    }
    
    setupBuffers();
    uploaded = true;
}

void Mesh::bind() const {
    if (!uploaded) {
        const_cast<Mesh*>(this)->upload();
    }
    
    glBindVertexArray(VAO);
}

void Mesh::unbind() const {
    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (!uploaded) {
        const_cast<Mesh*>(this)->upload();
    }
    
    bind();
    
    if (!indices.empty()) {
        glDrawElements(renderMode, indices.size(), GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(renderMode, 0, vertices.size());
    }
    
    unbind();
}

void Mesh::draw(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const {
    // Both Linux and Vita builds now use the lighting shader system
    // The material.apply() should have already set up the lighting shader
    
    if (!uploaded) {
        // Upload mesh data if not already uploaded
        const_cast<Mesh*>(this)->upload();
    }
    
    bind();
    
    if (!indices.empty()) {
        glDrawElements(renderMode, indices.size(), GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(renderMode, 0, vertices.size());
    }
    
    unbind();
}

void Mesh::draw(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const Material& material) const {
    if (!uploaded) {
        const_cast<Mesh*>(this)->upload();
    }
    
    bind();
    
    if (!indices.empty()) {
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
    }
    
    unbind();
}

std::shared_ptr<Mesh> Mesh::createQuad() {
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}
    };
    
    std::vector<unsigned int> indices = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->calculateTangents();
    mesh->setMeshType(MeshType::QUAD);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createPlane(float width, float height, int subdivisions) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    for (int y = 0; y <= subdivisions; ++y) {
        for (int x = 0; x <= subdivisions; ++x) {
            float xPos = (float)x / subdivisions * width - halfWidth;
            float zPos = (float)y / subdivisions * height - halfHeight;
            
            Vertex vertex;
            vertex.position = glm::vec3(xPos, 0.0f, zPos);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.texCoords = glm::vec2((float)x / subdivisions, (float)y / subdivisions);
            
            vertices.push_back(vertex);
        }
    }
    
    for (int y = 0; y < subdivisions; ++y) {
        for (int x = 0; x < subdivisions; ++x) {
            int topLeft = y * (subdivisions + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (y + 1) * (subdivisions + 1) + x;
            int bottomRight = bottomLeft + 1;
            
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->calculateTangents();
    mesh->setMeshType(MeshType::PLANE);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createCube() {
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},

        // Front face (Z+)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},

        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},

        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},

        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},

        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}
    };
    
    std::vector<unsigned int> indices;
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->calculateTangents();
    mesh->setMeshType(MeshType::CUBE);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createSphere(int segments, int rings, float radius) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= rings; y++) {
        float v = (float)y / rings;
        float phi = v * glm::pi<float>();

        for (int x = 0; x <= segments; x++) {
            float u = (float)x / segments;
            float theta = u * glm::two_pi<float>();

            float xPos = radius * sin(phi) * cos(theta);
            float yPos = radius * cos(phi);
            float zPos = radius * sin(phi) * sin(theta);

            glm::vec3 pos(xPos, yPos, zPos);
            glm::vec3 normal = glm::normalize(pos);

            vertices.push_back({pos, normal, glm::vec2(u, v)});
        }
    }

    for (int y = 0; y < rings; y++) {
        for (int x = 0; x < segments; x++) {
            int i0 = y * (segments + 1) + x;
            int i1 = i0 + 1;
            int i2 = i0 + (segments + 1);
            int i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->calculateTangents();
    mesh->setMeshType(MeshType::SPHERE);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createCapsule(float radius, float halfHeight, int segments, int rings) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= 1; y++) {
        float v = (float)y;
        float yPos = (v - 0.5f) * (2.0f * halfHeight);

        for (int x = 0; x <= segments; x++) {
            float u = (float)x / segments;
            float theta = u * glm::two_pi<float>();

            float xPos = radius * cos(theta);
            float zPos = radius * sin(theta);

            glm::vec3 pos(xPos, yPos, zPos);
            glm::vec3 normal = glm::normalize(glm::vec3(xPos, 0, zPos));

            vertices.push_back({pos, normal, glm::vec2(u, v)});
        }
    }

    int baseIndex = 0;
    for (int x = 0; x < segments; x++) {
        int i0 = baseIndex + x;
        int i1 = i0 + 1;
        int i2 = i0 + (segments + 1);
        int i3 = i2 + 1;

        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);

        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }

    auto addHemisphere = [&](float yOffset, bool flip) {
        int startIndex = vertices.size();
        for (int y = 0; y <= rings; y++) {
            float v = (float)y / rings;
            float phi = (glm::pi<float>() * 0.5f) * v;

            for (int x = 0; x <= segments; x++) {
                float u = (float)x / segments;
                float theta = u * glm::two_pi<float>();

                float xPos = radius * cos(theta) * sin(phi);
                float yPos = radius * cos(phi);
                float zPos = radius * sin(theta) * sin(phi);

                if (flip) yPos = -yPos;

                glm::vec3 pos(xPos, yPos + yOffset, zPos);
                glm::vec3 normal = glm::normalize(glm::vec3(xPos, yPos, zPos));

                vertices.push_back({pos, normal, glm::vec2(u, v)});
            }
        }

        for (int y = 0; y < rings; y++) {
            for (int x = 0; x < segments; x++) {
                int i0 = startIndex + y * (segments + 1) + x;
                int i1 = i0 + 1;
                int i2 = i0 + (segments + 1);
                int i3 = i2 + 1;

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }
    };

    addHemisphere(halfHeight, false); // top
    addHemisphere(-halfHeight, true); // bottom

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->calculateTangents();
    mesh->setMeshType(MeshType::CAPSULE);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createCylinder(float radius, float halfHeight, int segments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= 1; y++) {
        float v = (float)y;
        float yPos = (v - 0.5f) * (2.0f * halfHeight);

        for (int x = 0; x <= segments; x++) {
            float u = (float)x / segments;
            float theta = u * glm::two_pi<float>();

            float xPos = radius * cos(theta);
            float zPos = radius * sin(theta);

            glm::vec3 pos(xPos, yPos, zPos);
            glm::vec3 normal = glm::normalize(glm::vec3(xPos, 0, zPos));

            vertices.push_back({pos, normal, glm::vec2(u, v)});
        }
    }

    for (int x = 0; x < segments; x++) {
        int i0 = x;
        int i1 = i0 + 1;
        int i2 = i0 + (segments + 1);
        int i3 = i2 + 1;

        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);

        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }

    auto addDisk = [&](float yPos, float normalY) {
        int centerIndex = vertices.size();
        vertices.push_back({glm::vec3(0, yPos, 0), glm::vec3(0, normalY, 0), glm::vec2(0.5f, 0.5f)});
        for (int x = 0; x <= segments; x++) {
            float u = (float)x / segments;
            float theta = u * glm::two_pi<float>();
            float xPos = radius * cos(theta);
            float zPos = radius * sin(theta);
            vertices.push_back({glm::vec3(xPos, yPos, zPos), glm::vec3(0, normalY, 0), glm::vec2(u, 0)});
        }
        for (int x = 0; x < segments; x++) {
            indices.push_back(centerIndex);
            indices.push_back(centerIndex + x + 1);
            indices.push_back(centerIndex + x + 2);
        }
    };

    addDisk(halfHeight, 1);
    addDisk(-halfHeight, -1);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->calculateTangents();
    mesh->setMeshType(MeshType::CYLINDER);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::loadFromFile(const std::string& filepath) {
    // TODO: Implement file loading (OBJ, etc.)
    std::cerr << "Mesh::loadFromFile not yet implemented for: " << filepath << std::endl;
    return createCube(); // Return cube as placeholder
}

void Mesh::calculateBounds() {
    if (vertices.empty()) return;
    
    boundsMin = glm::vec3(std::numeric_limits<float>::max());
    boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    
    for (const auto& vertex : vertices) {
        boundsMin = glm::min(boundsMin, vertex.position);
        boundsMax = glm::max(boundsMax, vertex.position);
    }
}

void Mesh::calculateTangents() {
    if (vertices.empty() || indices.empty()) return;
    
    for (auto& vertex : vertices) {
        vertex.tangent = glm::vec3(0.0f);
    }
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        Vertex& v0 = vertices[indices[i]];
        Vertex& v1 = vertices[indices[i + 1]];
        Vertex& v2 = vertices[indices[i + 2]];
        
        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        
        glm::vec2 deltaUV1 = v1.texCoords - v0.texCoords;
        glm::vec2 deltaUV2 = v2.texCoords - v0.texCoords;
        
        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        
        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        
        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;
    }
    
    for (auto& vertex : vertices) {
        vertex.tangent = glm::normalize(vertex.tangent);
    }
}

void Mesh::setupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    
    if (!indices.empty()) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    }
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    
    glBindVertexArray(0);
}

void Mesh::drawDirectCube(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& color) const {
    draw();
}

void Mesh::drawDirectCube(const glm::vec3& color) const {
    draw();
}

std::shared_ptr<Mesh> Mesh::createWireframeBox(const glm::vec3& halfExtents) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float x = halfExtents.x, y = halfExtents.y, z = halfExtents.z;
    
    vertices.push_back({{-x, -y, -z}, {0, 0, 0}, {0, 0}}); // 0
    vertices.push_back({{ x, -y, -z}, {0, 0, 0}, {0, 0}}); // 1
    vertices.push_back({{ x, -y,  z}, {0, 0, 0}, {0, 0}}); // 2
    vertices.push_back({{-x, -y,  z}, {0, 0, 0}, {0, 0}}); // 3
    
    vertices.push_back({{-x,  y, -z}, {0, 0, 0}, {0, 0}}); // 4
    vertices.push_back({{ x,  y, -z}, {0, 0, 0}, {0, 0}}); // 5
    vertices.push_back({{ x,  y,  z}, {0, 0, 0}, {0, 0}}); // 6
    vertices.push_back({{-x,  y,  z}, {0, 0, 0}, {0, 0}}); // 7
    
    indices.insert(indices.end(), {0, 1, 1, 2, 2, 3, 3, 0});
    indices.insert(indices.end(), {4, 5, 5, 6, 6, 7, 7, 4});
    indices.insert(indices.end(), {0, 4, 1, 5, 2, 6, 3, 7});
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->setMeshType(MeshType::CUBE);
    mesh->setRenderMode(GL_LINES);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createWireframeSphere(float radius, int segments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    for (int i = 0; i <= segments; i++) {
        float phi = (float)i / segments * glm::two_pi<float>();
        for (int j = 0; j <= segments; j++) {
            float theta = (float)j / segments * glm::pi<float>();
            
            float x = radius * sin(theta) * cos(phi);
            float y = radius * cos(theta);
            float z = radius * sin(theta) * sin(phi);
            
            vertices.push_back({{x, y, z}, {0, 0, 0}, {0, 0}});
        }
    }
    
    for (int i = 0; i <= segments; i++) {
        for (int j = 0; j < segments; j++) {
            int current = i * (segments + 1) + j;
            int next = current + 1;
            indices.push_back(current);
            indices.push_back(next);
        }
    }
    
    for (int j = 0; j <= segments; j++) {
        for (int i = 0; i < segments; i++) {
            int current = i * (segments + 1) + j;
            int next = (i + 1) * (segments + 1) + j;
            indices.push_back(current);
            indices.push_back(next);
        }
    }
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->setMeshType(MeshType::SPHERE);
    mesh->setRenderMode(GL_LINES);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createWireframeCapsule(float radius, float height, int segments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float halfHeight = height * 0.5f;
    
    int capRings = 3;
    int topCapStart = vertices.size();
    for (int ring = 0; ring <= capRings; ring++) {
        float theta = (float)ring / capRings * glm::half_pi<float>();
        float ringRadius = radius * sin(theta);
        float y = radius * cos(theta) + halfHeight;
        
        for (int i = 0; i <= segments; i++) {
            float phi = (float)i / segments * glm::two_pi<float>();
            float x = ringRadius * cos(phi);
            float z = ringRadius * sin(phi);
            vertices.push_back({{x, y, z}, {0, 0, 0}, {0, 0}});
        }
    }
    
    int bottomCapStart = vertices.size();
    for (int ring = 0; ring <= capRings; ring++) {
        float theta = glm::half_pi<float>() + (float)ring / capRings * glm::half_pi<float>();
        float ringRadius = radius * sin(theta);
        float y = radius * cos(theta) - halfHeight;
        
        for (int i = 0; i <= segments; i++) {
            float phi = (float)i / segments * glm::two_pi<float>();
            float x = ringRadius * cos(phi);
            float z = ringRadius * sin(phi);
            vertices.push_back({{x, y, z}, {0, 0, 0}, {0, 0}});
        }
    }
    
    int cylinderTopStart = vertices.size();
    for (int i = 0; i <= segments; i++) {
        float phi = (float)i / segments * glm::two_pi<float>();
        float x = radius * cos(phi);
        float z = radius * sin(phi);
        vertices.push_back({{x, halfHeight, z}, {0, 0, 0}, {0, 0}});
    }
    
    int cylinderBottomStart = vertices.size();
    for (int i = 0; i <= segments; i++) {
        float phi = (float)i / segments * glm::two_pi<float>();
        float x = radius * cos(phi);
        float z = radius * sin(phi);
        vertices.push_back({{x, -halfHeight, z}, {0, 0, 0}, {0, 0}});
    }
    
    int verticesPerRing = segments + 1;
    
    for (int ring = 0; ring <= capRings; ring++) {
        int ringStart = topCapStart + ring * verticesPerRing;
        for (int i = 0; i < segments; i++) {
            indices.push_back(ringStart + i);
            indices.push_back(ringStart + i + 1);
        }
    }
    
    for (int i = 0; i <= segments; i++) {
        for (int ring = 0; ring < capRings; ring++) {
            int current = topCapStart + ring * verticesPerRing + i;
            int next = topCapStart + (ring + 1) * verticesPerRing + i;
            indices.push_back(current);
            indices.push_back(next);
        }
    }
    
    for (int ring = 0; ring <= capRings; ring++) {
        int ringStart = bottomCapStart + ring * verticesPerRing;
        for (int i = 0; i < segments; i++) {
            indices.push_back(ringStart + i);
            indices.push_back(ringStart + i + 1);
        }
    }
    
    for (int i = 0; i <= segments; i++) {
        for (int ring = 0; ring < capRings; ring++) {
            int current = bottomCapStart + ring * verticesPerRing + i;
            int next = bottomCapStart + (ring + 1) * verticesPerRing + i;
            indices.push_back(current);
            indices.push_back(next);
        }
    }
    
    for (int i = 0; i < segments; i++) {
        indices.push_back(cylinderTopStart + i);
        indices.push_back(cylinderTopStart + i + 1);
        indices.push_back(cylinderBottomStart + i);
        indices.push_back(cylinderBottomStart + i + 1);
    }
    
    for (int i = 0; i <= segments; i++) {
        indices.push_back(cylinderTopStart + i);
        indices.push_back(cylinderBottomStart + i);
    }
    
    // Connect caps to cylinder
    // Connect top cap bottom ring to cylinder top
    int topCapBottomRing = topCapStart + capRings * verticesPerRing;
    for (int i = 0; i <= segments; i++) {
        indices.push_back(topCapBottomRing + i);
        indices.push_back(cylinderTopStart + i);
    }
    
    int bottomCapTopRing = bottomCapStart;
    for (int i = 0; i <= segments; i++) {
        indices.push_back(bottomCapTopRing + i);
        indices.push_back(cylinderBottomStart + i);
    }
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->setMeshType(MeshType::CAPSULE);
    mesh->setRenderMode(GL_LINES);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createWireframeCylinder(float radius, float height, int segments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float halfHeight = height * 0.5f;
    
    for (int i = 0; i <= segments; i++) {
        float phi = (float)i / segments * glm::two_pi<float>();
        float x = radius * cos(phi);
        float z = radius * sin(phi);
        
        vertices.push_back({{x, halfHeight, z}, {0, 0, 0}, {0, 0}});
        vertices.push_back({{x, -halfHeight, z}, {0, 0, 0}, {0, 0}});
    }
    
    for (int i = 0; i < segments; i++) {
        int current = i * 2;
        int next = ((i + 1) % segments) * 2;
        
        indices.push_back(current);
        indices.push_back(next);
        
        indices.push_back(current + 1);
        indices.push_back(next + 1);
        
        indices.push_back(current);
        indices.push_back(current + 1);
    }
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->setMeshType(MeshType::CYLINDER);
    mesh->setRenderMode(GL_LINES);
    return mesh;
}

std::shared_ptr<Mesh> Mesh::createWireframePlane(float width, float height) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    vertices.push_back({{-halfWidth, 0, -halfHeight}, {0, 1, 0}, {0, 0}});
    vertices.push_back({{ halfWidth, 0, -halfHeight}, {0, 1, 0}, {1, 0}});
    vertices.push_back({{ halfWidth, 0,  halfHeight}, {0, 1, 0}, {1, 1}});
    vertices.push_back({{-halfWidth, 0,  halfHeight}, {0, 1, 0}, {0, 1}});
    
    indices.insert(indices.end(), {0, 1, 1, 2, 2, 3, 3, 0});
    
    indices.insert(indices.end(), {0, 2, 1, 3});
    
    auto mesh = std::make_shared<Mesh>(vertices, indices);
    mesh->setMeshType(MeshType::PLANE);
    mesh->setRenderMode(GL_LINES);
    return mesh;
}

void Mesh::cleanupBuffers() {
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    uploaded = false;
}

} // namespace GameEngine
