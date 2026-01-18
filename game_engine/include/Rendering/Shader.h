#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <memory>
#include "Platform.h"

namespace GameEngine {

class Shader {
public:
    Shader();
    ~Shader();
    
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool loadFromSource(const std::string& vertexSource, const std::string& fragmentSource);
    
    void use() const;
    void unuse() const;
    
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    void setBool(const std::string& name, bool value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setVec4Array(const std::string& name, const glm::vec4* values, size_t count);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    void setMat4Array(const std::string& name, const glm::mat4* values, size_t count);

    bool needsTranspose = false;
    
    GLuint getProgram() const { return program; }
    bool isValid() const { return program != 0; }
    
    static std::shared_ptr<Shader> getDefaultShader();
    static std::shared_ptr<Shader> getErrorShader();
    static std::shared_ptr<Shader> getLightingShader();
    
private:
    GLuint program;
    GLuint vertexShader;
    GLuint fragmentShader;
    
    mutable std::unordered_map<std::string, GLint> uniformCache;
    
    bool compileShader(GLuint& shader, GLenum type, const std::string& source);
    bool linkProgram();
    GLint getUniformLocation(const std::string& name) const;
    std::string readFile(const std::string& filepath);
};

} // namespace GameEngine

#endif // SHADER_H
