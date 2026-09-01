#include "Rendering/Shader.h"
#include "Core/AssetPaths.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

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
            std::cerr << "Shader: matrix array uniform '" << name << "' not found in this program." << std::endl;
        }
        return;
    }
    if (count == 0) return;

    #ifdef LINUX_BUILD
        glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_FALSE, &values[0][0][0]);
    #else
        glUniformMatrix4fv(location, static_cast<GLsizei>(count), needsTranspose ? GL_TRUE : GL_FALSE, &values[0][0][0]);
    #endif
}

std::shared_ptr<Shader> Shader::getDefaultShader() {
    static std::shared_ptr<Shader> defaultShader = nullptr;
    
    if (!defaultShader) {
        defaultShader = std::make_shared<Shader>();
        
#ifdef VITA_BUILD
        std::string vertexSource = R"(
            struct VertexInput {
                float3 aPosition : POSITION;
                float3 aNormal   : NORMAL;
                float2 aTexCoord : TEXCOORD0;
            };
            struct VertexOutput {
                float4 position  : POSITION;
                float3 FragPos  : TEXCOORD0;
                float3 Normal   : TEXCOORD1;
                float2 TexCoord : TEXCOORD2;
            };
            uniform float4x4 modelMatrix;
            uniform float4x4 viewMatrix;
            uniform float4x4 projectionMatrix;
            uniform float3x3 normalMatrix;
            VertexOutput main(VertexInput input) {
                VertexOutput output;
                float4 worldPos = mul(float4(input.aPosition, 1.0), modelMatrix);
                float4 viewPos = mul(worldPos, viewMatrix);
                output.position = mul(viewPos, projectionMatrix);
                output.FragPos = worldPos.xyz;
                output.Normal = normalize(mul(input.aNormal, normalMatrix));
                output.TexCoord = input.aTexCoord;
                return output;
            }
        )";
        std::string fragmentSource = R"(
            struct FragmentInput {
                float3 FragPos  : TEXCOORD0;
                float3 Normal   : TEXCOORD1;
                float2 TexCoord : TEXCOORD2;
            };
            uniform float3 u_DiffuseColor;
            float4 main(FragmentInput input) : COLOR {
                return float4(u_DiffuseColor, 1.0);
            }
        )";
#else
        std::string vertexSource = R"(
            #version 120
            attribute vec3 position;
            attribute vec3 normal;
            attribute vec2 texCoords;
            uniform mat4 modelMatrix;
            uniform mat4 viewMatrix;
            uniform mat4 projectionMatrix;
            uniform mat3 normalMatrix;
            varying vec3 FragPos;
            varying vec3 Normal;
            varying vec2 TexCoord;
            void main() {
                FragPos = vec3(modelMatrix * vec4(position, 1.0));
                Normal = normalMatrix * normal;
                TexCoord = texCoords;
                gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);
            }
        )";
        std::string fragmentSource = R"(
            #version 120
            varying vec3 FragPos;
            varying vec3 Normal;
            varying vec2 TexCoord;
            uniform vec3 u_DiffuseColor;
            void main() {
                gl_FragColor = vec4(u_DiffuseColor, 1.0);
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

    static bool loadAttempted = false;

    if (!loadAttempted) {
        loadAttempted = true;
        lightingShader = std::make_shared<Shader>();

#ifdef LINUX_BUILD
        if (lightingShader->loadFromFiles("assets/linux_shaders/lighting.vert", "assets/linux_shaders/lighting.frag")) {
            lightingShader->use();
            GLint boneMatLoc = glGetUniformLocation(lightingShader->getProgram(), "u_BoneMatrices");
            GLint numBonesLoc = glGetUniformLocation(lightingShader->getProgram(), "u_NumBones");
            if (boneMatLoc == -1) {
                std::cerr << "ERROR: u_BoneMatrices uniform NOT FOUND in EXTERNAL shader (assets/linux_shaders/lighting.vert)" << std::endl;
                std::cerr << "The shader file needs bone animation support added!" << std::endl;
            }
            if (numBonesLoc == -1) {
                std::cerr << "ERROR: u_NumBones uniform NOT FOUND in EXTERNAL shader" << std::endl;
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
            uniform float u_Opacity;
            
            void main() {
                vec3 normal = normalize(vNormal);
                vec3 viewDir = normalize(u_CameraPos - vWorldPos);
                
                // Simple ambient lighting
                vec3 ambient = vec3(0.1) * u_DiffuseColor;
                vec3 result = ambient + u_DiffuseColor;
                
                gl_FragColor = vec4(result, u_Opacity);
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
        if (lightingShader->loadFromFiles("assets/shaders/lighting.vert", "assets/shaders/lighting.frag")) {
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

std::shared_ptr<Shader> Shader::getShadowDepthShader() {
    static std::shared_ptr<Shader> shadowShader = nullptr;
    static bool loadAttempted = false;

    // A failed load must not be retried every frame: on Vita a shader compile
    // goes through the runtime Cg compiler and is far too slow for that
    if (loadAttempted) {
        return shadowShader;
    }
    loadAttempted = true;

    shadowShader = std::make_shared<Shader>();

#ifdef VITA_BUILD
    const char* vertexPath = "assets/shaders/shadow_depth.vert";
    const char* fragmentPath = "assets/shaders/shadow_depth.frag";
#else
    const char* vertexPath = "assets/linux_shaders/shadow_depth.vert";
    const char* fragmentPath = "assets/linux_shaders/shadow_depth.frag";
#endif

    if (!shadowShader->loadFromFiles(vertexPath, fragmentPath)) {
        std::cerr << "Shader: shadow depth shader not found, shadows disabled" << std::endl;
        shadowShader.reset();
    }

    return shadowShader;
}

std::shared_ptr<Shader> Shader::getLightingInstancedShader() {
    static std::shared_ptr<Shader> instancedShader = nullptr;
    static bool loadAttempted = false;

    if (loadAttempted) {
        return instancedShader;
    }
    loadAttempted = true;

    instancedShader = std::make_shared<Shader>();

#ifdef VITA_BUILD
    const char* vertexPath = "assets/shaders/lighting_instanced.vert";
    const char* fragmentPath = "assets/shaders/lighting.frag";
#else
    const char* vertexPath = "assets/linux_shaders/lighting_instanced.vert";
    const char* fragmentPath = "assets/linux_shaders/lighting.frag";
#endif

    if (!instancedShader->loadFromFiles(vertexPath, fragmentPath)) {
        std::cerr << "Shader: instanced lighting shader not found, GPU instancing disabled"
                  << " (falling back to per-entity draws)" << std::endl;
        instancedShader.reset();
    }

    return instancedShader;
}

std::shared_ptr<Shader> Shader::getTextShader() {
    static std::shared_ptr<Shader> textShader = nullptr;

    if (!textShader) {
        textShader = std::make_shared<Shader>();

#ifdef VITA_BUILD
        textShader->needsTranspose = true;
        if (textShader->loadFromFiles("assets/shaders/text.vert", "assets/shaders/text.frag")) {
            return textShader;
        }

        const std::string vertexSource = R"(
            struct VS_INPUT {
                float3 aPosition : POSITION;
                float2 aTexCoord : TEXCOORD0;
            };

            struct VS_OUTPUT {
                float4 Position : POSITION;
                float2 texCoord : TEXCOORD0;
            };

            float4x4 uViewProjectionMat;
            float4x4 uModelMat;

            VS_OUTPUT main(VS_INPUT input) {
                VS_OUTPUT output;
                output.Position = mul(uViewProjectionMat, mul(uModelMat, float4(input.aPosition, 1.0)));
                output.texCoord = input.aTexCoord;
                return output;
            }
        )";

        const std::string fragmentSource = R"(
            struct PS_INPUT {
                float2 texCoord : TEXCOORD0;
            };

            sampler2D uFontAtlasTexture;
            uniform float4 uColor;

            float4 main(PS_INPUT input) : COLOR {
                float alpha = tex2D(uFontAtlasTexture, input.texCoord).r;
                return float4(uColor.rgb, uColor.a * alpha);
            }
        )";

        if (!textShader->loadFromSource(vertexSource, fragmentSource)) {
            textShader.reset();
        }
#elif defined(LINUX_BUILD)
        textShader->needsTranspose = false;
        if (textShader->loadFromFiles("assets/linux_shaders/text.vert", "assets/linux_shaders/text.frag")) {
            return textShader;
        }

        const std::string vertexSource = R"(
            #version 120
            attribute vec3 aPosition;
            attribute vec2 aTexCoord;

            uniform mat4 uViewProjectionMat;
            uniform mat4 uModelMat;

            varying vec2 texCoord;

            void main()
            {
                gl_Position = uViewProjectionMat * uModelMat * vec4(aPosition, 1.0);
                texCoord = aTexCoord;
            }
        )";

        const std::string fragmentSource = R"(
            #version 120
            varying vec2 texCoord;

            uniform sampler2D uFontAtlasTexture;
            uniform vec4 uColor;

            void main()
            {
                float alpha = texture2D(uFontAtlasTexture, texCoord).r;
                gl_FragColor = vec4(uColor.rgb, uColor.a * alpha);
            }
        )";

        if (!textShader->loadFromSource(vertexSource, fragmentSource)) {
            textShader.reset();
        }
#endif
    }

    return textShader;
}

bool Shader::compileShader(GLuint& shader, GLenum type, const std::string& source) {
#ifdef ENABLE_VULKAN
    (void)type;
    (void)source;
    shader = 0;
    static bool warned = false;
    if (!warned) {
        std::cerr << "Shader::compileShader called in Vulkan build, skipping OpenGL shader compilation." << std::endl;
        warned = true;
    }
    return false;
#endif

    shader = glCreateShader(type);
    const char* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[2048];
        infoLog[0] = '\0';
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        const char* stage = (type == GL_VERTEX_SHADER ? "vertex" : "fragment");
        std::cerr << "Shader compilation failed (" << stage << "):" << std::endl;
        std::cerr << infoLog << std::endl;

#ifdef VITA_BUILD
        sceClibPrintf("=== %s shader failed ===\n%s\n", stage, infoLog);
#endif

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

    glBindAttribLocation(program, 4, "iModelCol0");
    glBindAttribLocation(program, 5, "iModelCol1");
    glBindAttribLocation(program, 6, "iModelCol2");
    glBindAttribLocation(program, 7, "iModelCol3");
    glBindAttribLocation(program, 8, "iNormalCol0");
    glBindAttribLocation(program, 9, "iNormalCol1");
    glBindAttribLocation(program, 10, "iNormalCol2");

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

    if (location == -1 && name.find('[') == std::string::npos) {
        const std::string firstElement = name + "[0]";
        location = glGetUniformLocation(program, firstElement.c_str());
    }

    uniformCache[name] = location;
    return location;
}

std::string Shader::readFile(const std::string& filepath) {
    std::ifstream file(AssetPaths::resolve(filepath));
    if (!file.is_open()) {
        
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace GameEngine
