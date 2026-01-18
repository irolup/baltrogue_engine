#include "Rendering/Shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace GameEngine {

Shader::Shader()
    : program(0), vertexShader(0), fragmentShader(0)
{
}

Shader::~Shader() {
    if (program) {
        glDeleteProgram(program);
    }
    if (vertexShader) {
        glDeleteShader(vertexShader);
    }
    if (fragmentShader) {
        glDeleteShader(fragmentShader);
    }
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSource = readFile(vertexPath);
    std::string fragmentSource = readFile(fragmentPath);
    
    if (vertexSource.empty() || fragmentSource.empty()) {
        return false;
    }
    
    return loadFromSource(vertexSource, fragmentSource);
}

bool Shader::loadFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
    // Compile vertex shader
    if (!compileShader(vertexShader, GL_VERTEX_SHADER, vertexSource)) {
        return false;
    }
    
    // Compile fragment shader
    if (!compileShader(fragmentShader, GL_FRAGMENT_SHADER, fragmentSource)) {
        return false;
    }
    
    // Link program
    return linkProgram();
}

void Shader::use() const {
    if (program) {
        glUseProgram(program);
    }
}

void Shader::unuse() const {
    glUseProgram(0);
}

void Shader::setFloat(const std::string& name, float value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void Shader::setInt(const std::string& name, int value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Shader::setBool(const std::string& name, bool value) {
    setInt(name, value ? 1 : 0);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform2fv(location, 1, &value[0]);
    }
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, 1, &value[0]);
    }
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4fv(location, 1, &value[0]);
    }
}

void Shader::setVec4Array(const std::string& name, const glm::vec4* values, size_t count) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4fv(location, count, &values[0][0]);
    }
}

void Shader::setMat3(const std::string& name, const glm::mat3& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix3fv(location, 1, GL_FALSE, &value[0][0]);
    }
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) {
    GLint location = getUniformLocation(name);
    if (location == -1) return;

    #ifdef LINUX_BUILD
        glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]); // always column-major
    #else
        glUniformMatrix4fv(location, 1, needsTranspose ? GL_TRUE : GL_FALSE, &value[0][0]);
    #endif
}

void Shader::setMat4Array(const std::string& name, const glm::mat4* values, size_t count) {
    GLint location = getUniformLocation(name);
    if (location == -1) {
        static int warnCount = 0;
        warnCount++;
        if (warnCount % 60 == 0) {  // Log every 60 frames
            std::cerr << "Shader: Uniform '" << name << "' not found! Shader may not support bone matrices." << std::endl;
        }
        return;
    }
    if (count == 0) return;

    #ifdef LINUX_BUILD
        glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_FALSE, &values[0][0][0]);
    #else
        glUniformMatrix4fv(location, static_cast<GLsizei>(count), needsTranspose ? GL_TRUE : GL_FALSE, &values[0][0][0]);
    #endif
    
    static int setCount = 0;
    setCount++;
    if (setCount % 60 == 0) {  // Log every 60 frames
        std::cout << "Shader: Set uniform '" << name << "' with " << count << " matrices (location=" << location << ")" << std::endl;
    }
}

std::shared_ptr<Shader> Shader::getDefaultShader() {
    static std::shared_ptr<Shader> defaultShader = nullptr;
    
    if (!defaultShader) {
        defaultShader = std::make_shared<Shader>();
        
#ifdef LINUX_BUILD
        // Linux builds use GLSL shaders
        std::string vertexSource = R"(
            #version 120
            attribute vec3 aPos;
            attribute vec3 aNormal;
            attribute vec2 aTexCoord;
            
            uniform mat4 modelMatrix;
            uniform mat4 viewMatrix;
            uniform mat4 projectionMatrix;
            uniform mat3 normalMatrix;
            
            varying vec3 FragPos;
            varying vec3 Normal;
            varying vec2 TexCoord;
            
            void main() {
                FragPos = vec3(modelMatrix * vec4(aPos, 1.0));
                Normal = normalMatrix * aNormal;
                TexCoord = aTexCoord;
                
                gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);
            }
        )";
        
        std::string fragmentSource = R"(
            #version 120
            varying vec3 FragPos;
            varying vec3 Normal;
            varying vec2 TexCoord;
            
            uniform vec3 diffuseColor;
            
            void main() {
                // Use flat lighting - no directional lighting to ensure consistency across platforms
                vec3 color = diffuseColor;
                gl_FragColor = vec4(color, 1.0);
            }
        )";
        
        if (!defaultShader->loadFromSource(vertexSource, fragmentSource)) {
            std::cerr << "Failed to load default shader!" << std::endl;
            defaultShader.reset();
        }
#else
        // Vita builds use CG/HLSL shaders
        std::string vertexSource = R"(
            struct VS_INPUT {
                float3 aPos : POSITION;
                float3 aNormal : NORMAL;
                float2 aTexCoord : TEXCOORD0;
            };
            
            struct VS_OUTPUT {
                float4 Position : POSITION;
                float3 FragPos : TEXCOORD0;
                float3 Normal : TEXCOORD1;
                float2 TexCoord : TEXCOORD2;
            };
            
            float4x4 modelMatrix;
            float4x4 viewMatrix;
            float4x4 projectionMatrix;
            float3x3 normalMatrix;
            
            VS_OUTPUT main(VS_INPUT input) {
                VS_OUTPUT output;
                
                float4 worldPos = mul(modelMatrix, float4(input.aPos, 1.0));
                output.FragPos = worldPos.xyz;
                output.Normal = mul(normalMatrix, input.aNormal);
                output.TexCoord = input.aTexCoord;
                
                output.Position = mul(projectionMatrix, mul(viewMatrix, worldPos));
                return output;
            }
        )";
        
        std::string fragmentSource = R"(
            struct PS_INPUT {
                float3 FragPos : TEXCOORD0;
                float3 Normal : TEXCOORD1;
                float2 TexCoord : TEXCOORD2;
            };
            
            float3 diffuseColor;
            
            float4 main(PS_INPUT input) : COLOR {
                // Use flat lighting - no directional lighting to ensure consistency across platforms
                float3 color = diffuseColor;
                return float4(color, 1.0);
            }
        )";
#endif
        
        if (!defaultShader->loadFromSource(vertexSource, fragmentSource)) {
            std::cerr << "Failed to load default shader!" << std::endl;
            defaultShader.reset();
        }
    }
    
    return defaultShader;
}

std::shared_ptr<Shader> Shader::getErrorShader() {
    static std::shared_ptr<Shader> errorShader = nullptr;
    
    if (!errorShader) {
        errorShader = std::make_shared<Shader>();
        
#ifdef LINUX_BUILD
        // Linux builds use GLSL shaders
        std::string vertexSource = R"(
            #version 120
            attribute vec3 aPos;
            
            uniform mat4 modelMatrix;
            uniform mat4 viewMatrix;
            uniform mat4 projectionMatrix;
            
            void main() {
                gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);
            }
        )";
        
        std::string fragmentSource = R"(
            #version 120
            void main() {
                gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta
            }
        )";
#else
        // Vita builds use CG/HLSL shaders
        std::string vertexSource = R"(
            struct VS_INPUT {
                float3 aPos : POSITION;
            };
            
            float4x4 modelMatrix;
            float4x4 viewMatrix;
            float4x4 projectionMatrix;
            
            float4 main(VS_INPUT input) : POSITION {
                return mul(projectionMatrix, mul(viewMatrix, mul(modelMatrix, float4(input.aPos, 1.0))));
            }
        )";
        
        std::string fragmentSource = R"(
            float4 main() : COLOR {
                return float4(1.0, 0.0, 1.0, 1.0); // Magenta
            }
        )";
#endif
        
        if (!errorShader->loadFromSource(vertexSource, fragmentSource)) {
            errorShader.reset();
        }
    }
    
    return errorShader;
}

std::shared_ptr<Shader> Shader::getLightingShader() {
    static std::shared_ptr<Shader> lightingShader = nullptr;
    
    // Force recreation to pick up changes (for debugging)
    // Remove this after confirming it works
    // lightingShader = nullptr;
    
    if (!lightingShader) {
        lightingShader = std::make_shared<Shader>();
        
#ifdef LINUX_BUILD
        // Try to load the external lighting shaders first
        // Try multiple possible paths for the editor
        std::cout << "Trying to load external lighting shaders..." << std::endl;
        
        if (lightingShader->loadFromFiles("assets/linux_shaders/lighting.vert", "assets/linux_shaders/lighting.frag")) {
            std::cout << "Successfully loaded EXTERNAL lighting shaders from assets/linux_shaders/" << std::endl;
            // Verify bone matrix uniform exists in external shader
            lightingShader->use();
            GLint boneMatLoc = glGetUniformLocation(lightingShader->getProgram(), "u_BoneMatrices");
            GLint numBonesLoc = glGetUniformLocation(lightingShader->getProgram(), "u_NumBones");
            if (boneMatLoc == -1) {
                std::cerr << "ERROR: u_BoneMatrices uniform NOT FOUND in EXTERNAL shader (assets/linux_shaders/lighting.vert)" << std::endl;
                std::cerr << "The shader file needs bone animation support added!" << std::endl;
            } else {
                std::cout << "EXTERNAL shader: u_BoneMatrices found at location " << boneMatLoc << std::endl;
            }
            if (numBonesLoc == -1) {
                std::cerr << "ERROR: u_NumBones uniform NOT FOUND in EXTERNAL shader" << std::endl;
            } else {
                std::cout << "EXTERNAL shader: u_NumBones found at location " << numBonesLoc << std::endl;
            }
            lightingShader->unuse();
            return lightingShader;
        }
        
        // Try relative to current directory
        if (lightingShader->loadFromFiles("./assets/linux_shaders/lighting.vert", "./assets/linux_shaders/lighting.frag")) {
            std::cout << "Successfully loaded EXTERNAL lighting shaders from ./assets/linux_shaders/" << std::endl;
            // Verify bone matrix uniform exists
            lightingShader->use();
            GLint boneMatLoc = glGetUniformLocation(lightingShader->getProgram(), "u_BoneMatrices");
            GLint numBonesLoc = glGetUniformLocation(lightingShader->getProgram(), "u_NumBones");
            if (boneMatLoc == -1) {
                std::cerr << "WARNING: u_BoneMatrices uniform NOT FOUND in EXTERNAL shader (./assets/linux_shaders/lighting.vert)" << std::endl;
            } else {
                std::cout << "EXTERNAL shader: u_BoneMatrices found at location " << boneMatLoc << std::endl;
            }
            lightingShader->unuse();
            return lightingShader;
        }
        
        // Try from project root
        if (lightingShader->loadFromFiles("../assets/linux_shaders/lighting.vert", "../assets/linux_shaders/lighting.frag")) {
            std::cout << "Successfully loaded EXTERNAL lighting shaders from ../assets/linux_shaders/" << std::endl;
            // Verify bone matrix uniform exists
            lightingShader->use();
            GLint boneMatLoc = glGetUniformLocation(lightingShader->getProgram(), "u_BoneMatrices");
            if (boneMatLoc == -1) {
                std::cerr << "WARNING: u_BoneMatrices uniform NOT FOUND in EXTERNAL shader" << std::endl;
            } else {
                std::cout << "EXTERNAL shader: u_BoneMatrices found at location " << boneMatLoc << std::endl;
            }
            lightingShader->unuse();
            return lightingShader;
        }
        
        std::cout << "External lighting shaders not found, using embedded fallback" << std::endl;
        
        // Fallback to embedded lighting shader if files can't be loaded
        // Note: Uses attribute names that match Mesh::setupBuffers() binding
        std::string vertexSource = R"(
            #version 120
            attribute vec3 position;
            attribute vec3 normal;
            attribute vec2 texCoords;
            attribute vec3 tangent;
            attribute vec4 boneWeights;
            attribute vec4 boneIndices;
            
            uniform mat4 modelMatrix;
            uniform mat4 viewMatrix;
            uniform mat4 projectionMatrix;
            uniform mat3 normalMatrix;
            
            // Bone animation uniforms
            uniform mat4 u_BoneMatrices[100];
            uniform int u_NumBones;
            
            varying vec3 vWorldPos;
            varying vec3 vNormal;
            varying vec2 vTexCoord;
            varying vec3 vViewPos;
            
            void main() {
                vec4 skinnedPosition = vec4(position, 1.0);
                vec3 skinnedNormal = normal;
                
                // Apply bone skinning if bones are available
                if (u_NumBones > 0 && boneWeights.x > 0.0) {
                    // Get bone indices (convert from float to int)
                    // GLSL 120/Cg: Use ivec4 to convert all at once, then extract components
                    ivec4 boneIndicesInt = ivec4(floor(boneIndices + 0.5));
                    int boneIndex0 = boneIndicesInt.x;
                    int boneIndex1 = boneIndicesInt.y;
                    int boneIndex2 = boneIndicesInt.z;
                    int boneIndex3 = boneIndicesInt.w;
                    
                    // Clamp bone indices to valid range (max 99 to match array size)
                    int maxBoneIndex = u_NumBones - 1;
                    if (maxBoneIndex > 99) maxBoneIndex = 99;
                    // Manual clamping for GLSL 120 compatibility (clamp() may return float)
                    if (boneIndex0 < 0) boneIndex0 = 0;
                    if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
                    if (boneIndex1 < 0) boneIndex1 = 0;
                    if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
                    if (boneIndex2 < 0) boneIndex2 = 0;
                    if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
                    if (boneIndex3 < 0) boneIndex3 = 0;
                    if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
                    
                    // Apply bone transformations (blend matrices correctly)
                    // Each bone transform is applied separately and then blended
                    vec4 pos0 = u_BoneMatrices[boneIndex0] * vec4(position, 1.0);
                    vec4 pos1 = u_BoneMatrices[boneIndex1] * vec4(position, 1.0);
                    vec4 pos2 = u_BoneMatrices[boneIndex2] * vec4(position, 1.0);
                    vec4 pos3 = u_BoneMatrices[boneIndex3] * vec4(position, 1.0);
                    
                    // Blend the transformed positions
                    skinnedPosition = pos0 * boneWeights.x + pos1 * boneWeights.y + pos2 * boneWeights.z + pos3 * boneWeights.w;
                    
                    // Transform normal (blend separately)
                    vec3 norm0 = normalize(mat3(u_BoneMatrices[boneIndex0]) * normal);
                    vec3 norm1 = normalize(mat3(u_BoneMatrices[boneIndex1]) * normal);
                    vec3 norm2 = normalize(mat3(u_BoneMatrices[boneIndex2]) * normal);
                    vec3 norm3 = normalize(mat3(u_BoneMatrices[boneIndex3]) * normal);
                    skinnedNormal = normalize(norm0 * boneWeights.x + norm1 * boneWeights.y + norm2 * boneWeights.z + norm3 * boneWeights.w);
                }
                
                vec4 worldPos = modelMatrix * skinnedPosition;
                vWorldPos = worldPos.xyz;
                
                vec4 viewPos = viewMatrix * worldPos;
                vViewPos = viewPos.xyz;
                
                vNormal = normalize(normalMatrix * skinnedNormal);
                vTexCoord = texCoords;
                
                gl_Position = projectionMatrix * viewPos;
            }
        )";
        
        std::string fragmentSource = R"(
            #version 120
            varying vec3 vWorldPos;
            varying vec3 vNormal;
            varying vec2 vTexCoord;
            varying vec3 vViewPos;
            
            uniform vec3 u_DiffuseColor;
            uniform vec3 u_CameraPos;
            
            void main() {
                vec3 normal = normalize(vNormal);
                vec3 viewDir = normalize(u_CameraPos - vWorldPos);
                
                // Simple ambient lighting
                vec3 ambient = vec3(0.1) * u_DiffuseColor;
                vec3 result = ambient + u_DiffuseColor;
                
                gl_FragColor = vec4(result, 1.0);
            }
        )";
        
        if (!lightingShader->loadFromSource(vertexSource, fragmentSource)) {
            std::cerr << "Failed to load embedded lighting shader!" << std::endl;
            lightingShader.reset();
        } else {
            std::cout << "Using embedded lighting shader (external files not found)" << std::endl;
            // Verify bone matrix uniform exists
            lightingShader->use();
            GLint boneMatLoc = glGetUniformLocation(lightingShader->getProgram(), "u_BoneMatrices");
            GLint numBonesLoc = glGetUniformLocation(lightingShader->getProgram(), "u_NumBones");
            if (boneMatLoc == -1) {
                std::cerr << "WARNING: u_BoneMatrices uniform not found in embedded lighting shader!" << std::endl;
            } else {
                std::cout << "Embedded lighting shader: u_BoneMatrices found at location " << boneMatLoc << std::endl;
            }
            if (numBonesLoc == -1) {
                std::cerr << "WARNING: u_NumBones uniform not found in embedded lighting shader!" << std::endl;
            } else {
                std::cout << "Embedded lighting shader: u_NumBones found at location " << numBonesLoc << std::endl;
            }
            lightingShader->unuse();
        }
    #else
        // Vita builds use CG/HLSL shaders
        // Try from app root (app0:)
        if (lightingShader->loadFromFiles("app0:/lighting.vert", "app0:/lighting.frag")) {
            return lightingShader;
        }
        
        // No fallback shader for Vita - return null if external files not found
        lightingShader.reset();
        
#endif
    }
    
    // Safety check - ensure we have a valid shader
    if (!lightingShader || !lightingShader->isValid()) {
        return getDefaultShader();
    }
    
    return lightingShader;
}

bool Shader::compileShader(GLuint& shader, GLenum type, const std::string& source) {
    shader = glCreateShader(type);
    const char* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "Shader compilation failed (" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << "):" << std::endl;
        std::cerr << infoLog << std::endl;
        
        glDeleteShader(shader);
        shader = 0;
        return false;
    }
    
    return true;
}

bool Shader::linkProgram() {
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    
    // Bind attribute locations (for GLSL 120 compatibility)
    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "normal");
    glBindAttribLocation(program, 2, "texCoords");
    glBindAttribLocation(program, 3, "tangent");
    glBindAttribLocation(program, 4, "boneWeights");
    glBindAttribLocation(program, 5, "boneIndices");
    
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    
    return true;
}

GLint Shader::getUniformLocation(const std::string& name) const {
    auto it = uniformCache.find(name);
    if (it != uniformCache.end()) {
        return it->second;
    }
    
    GLint location = glGetUniformLocation(program, name.c_str());
    uniformCache[name] = location;
    return location;
}

std::string Shader::readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace GameEngine
