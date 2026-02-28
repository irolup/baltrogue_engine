#include "Rendering/ShaderManager.h"
#include "Rendering/Shader.h"
#include "Platform.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>

#ifdef LINUX_BUILD
    #include <filesystem>
#endif

namespace GameEngine {

ShaderManager& ShaderManager::getInstance() {
    static ShaderManager instance;
    return instance;
}

std::string ShaderManager::getShaderDirectory() {
#ifdef VITA_BUILD
    return "app0:/assets/shaders";
#else
    return "assets/linux_shaders";
#endif
}

std::string ShaderManager::getShaderDirectory(const std::string& platform) {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        return "assets/shaders";
    } else if (platform == "linux" || platform == "Linux" || platform == "LINUX") {
        return "assets/linux_shaders";
    } else {
        return getShaderDirectory();
    }
}

std::vector<std::string> ShaderManager::discoverShaders(const std::string& directory, const std::string& extension) {
    std::vector<std::string> shaders;
    
#ifdef LINUX_BUILD
    try {
        if (!std::filesystem::exists(directory)) {
            return shaders;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.length() >= extension.length() && 
                    filename.substr(filename.length() - extension.length()) == extension) {
                    shaders.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error discovering shaders in " << directory << ": " << e.what() << std::endl;
    }
#else

#endif
    
    return shaders;
}

std::string ShaderManager::getShaderCacheKey(const std::string& vertexPath, const std::string& fragmentPath) const {
    return vertexPath + "|" + fragmentPath;
}

bool ShaderManager::writeShaderFile(const std::string& filepath, const std::string& content) {
#ifdef LINUX_BUILD
    std::filesystem::path path(filepath);
    std::filesystem::path dir = path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
#endif
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to create shader file: " << filepath << std::endl;
        return false;
    }
    
    file << content;
    file.close();
    return true;
}

std::string ShaderManager::generateLitVertexShader(bool isVita) {
    if (isVita) {
        return R"(// CG Vertex Shader
// Custom Lit Shader

struct VertexInput {
    float3 aPosition : POSITION;
    float3 aNormal   : NORMAL;
    float2 aTexCoord : TEXCOORD0;
    float3 aTangent  : TEXCOORD1;
    float4 boneWeights : TEXCOORD2;
    float4 boneIndices : TEXCOORD3;
};

struct VertexOutput {
    float4 position  : POSITION;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 texCoord  : TEXCOORD2;
    float3 viewPos   : TEXCOORD3;
    float3 tangent   : TEXCOORD4;
    float3 bitangent : TEXCOORD5;
};

uniform float4x4 modelMatrix;
uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;
uniform float3x3 normalMatrix;

uniform float4x4 u_BoneMatrices[100];
uniform int u_NumBones;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    float4 skinnedPosition = float4(input.aPosition, 1.0f);
    float3 skinnedNormal = input.aNormal;
    float3 skinnedTangent = input.aTangent;

    if (u_NumBones > 0) {
        int boneIndex0 = (int)input.boneIndices.x;
        int boneIndex1 = (int)input.boneIndices.y;
        int boneIndex2 = (int)input.boneIndices.z;
        int boneIndex3 = (int)input.boneIndices.w;
        
        int maxBoneIndex = u_NumBones - 1;
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        float4x4 boneTransform = 
            u_BoneMatrices[boneIndex0] * input.boneWeights.x +
            u_BoneMatrices[boneIndex1] * input.boneWeights.y +
            u_BoneMatrices[boneIndex2] * input.boneWeights.z +
            u_BoneMatrices[boneIndex3] * input.boneWeights.w;
        
        skinnedPosition = mul(float4(input.aPosition, 1.0f), boneTransform);
        
        float3x3 boneNormalMatrix = float3x3(
            boneTransform[0].xyz,
            boneTransform[1].xyz,
            boneTransform[2].xyz
        );
        skinnedNormal = normalize(mul(input.aNormal, boneNormalMatrix));
        skinnedTangent = normalize(mul(input.aTangent, boneNormalMatrix));
    }
    
    float4 worldPos = mul(skinnedPosition, modelMatrix);
    output.worldPos = worldPos.xyz;
    
    float4 viewPos = mul(worldPos, viewMatrix);
    output.viewPos = viewPos.xyz;
    
    output.normal = normalize(mul(skinnedNormal, normalMatrix));
    output.tangent = normalize(mul(skinnedTangent, normalMatrix));
    output.bitangent = normalize(cross(output.normal, output.tangent));
    
    output.texCoord = input.aTexCoord;
    output.position = mul(viewPos, projectionMatrix);
    
    return output;
})";
    } else {
        return R"(#version 120
// GLSL Vertex Shader
// Custom Lit Shader

attribute vec3 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;
attribute vec4 boneWeights;
attribute vec4 boneIndices;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;

uniform mat4 u_BoneMatrices[100];
uniform int u_NumBones;

varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;
varying vec3 vViewPos;
varying vec3 vTangent;
varying vec3 vBitangent;

void main() {
    vec4 skinnedPosition = vec4(position, 1.0);
    vec3 skinnedNormal = normal;
    vec3 skinnedTangent = tangent;
    
    if (u_NumBones > 0 && boneWeights.x > 0.0) {
        ivec4 boneIndicesInt = ivec4(floor(boneIndices + 0.5));
        int boneIndex0 = boneIndicesInt.x;
        int boneIndex1 = boneIndicesInt.y;
        int boneIndex2 = boneIndicesInt.z;
        int boneIndex3 = boneIndicesInt.w;
        
        int maxBoneIndex = u_NumBones - 1;
        if (maxBoneIndex > 99) maxBoneIndex = 99;
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        vec4 pos0 = u_BoneMatrices[boneIndex0] * vec4(position, 1.0);
        vec4 pos1 = u_BoneMatrices[boneIndex1] * vec4(position, 1.0);
        vec4 pos2 = u_BoneMatrices[boneIndex2] * vec4(position, 1.0);
        vec4 pos3 = u_BoneMatrices[boneIndex3] * vec4(position, 1.0);
        
        skinnedPosition = pos0 * boneWeights.x + pos1 * boneWeights.y + pos2 * boneWeights.z + pos3 * boneWeights.w;
        
        vec3 norm0 = normalize(mat3(u_BoneMatrices[boneIndex0]) * normal);
        vec3 norm1 = normalize(mat3(u_BoneMatrices[boneIndex1]) * normal);
        vec3 norm2 = normalize(mat3(u_BoneMatrices[boneIndex2]) * normal);
        vec3 norm3 = normalize(mat3(u_BoneMatrices[boneIndex3]) * normal);
        skinnedNormal = normalize(norm0 * boneWeights.x + norm1 * boneWeights.y + norm2 * boneWeights.z + norm3 * boneWeights.w);
        
        vec3 tan0 = normalize(mat3(u_BoneMatrices[boneIndex0]) * tangent);
        vec3 tan1 = normalize(mat3(u_BoneMatrices[boneIndex1]) * tangent);
        vec3 tan2 = normalize(mat3(u_BoneMatrices[boneIndex2]) * tangent);
        vec3 tan3 = normalize(mat3(u_BoneMatrices[boneIndex3]) * tangent);
        skinnedTangent = normalize(tan0 * boneWeights.x + tan1 * boneWeights.y + tan2 * boneWeights.z + tan3 * boneWeights.w);
    }
    
    vec4 worldPos = modelMatrix * skinnedPosition;
    vWorldPos = worldPos.xyz;
    
    vec4 viewPos = viewMatrix * worldPos;
    vViewPos = viewPos.xyz;
    
    vNormal = normalize(normalMatrix * skinnedNormal);
    vTangent = normalize(normalMatrix * skinnedTangent);
    vBitangent = normalize(cross(vNormal, vTangent));
    
    vTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);
    gl_Position = projectionMatrix * viewPos;
})";
    }
}

std::string ShaderManager::generateLitFragmentShader(bool isVita) {
    if (isVita) {
        return R"(// CG Fragment Shader
// Custom Lit Shader

struct FragmentInput {
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float3 viewPos  : TEXCOORD3;
    float3 tangent  : TEXCOORD4;
    float3 bitangent: TEXCOORD5;
};

struct Light {
    float4 position;
    float4 direction;
    float4 color;
    float4 params;
    float4 attenuation;
};

uniform int u_NumLights;
uniform Light u_Lights[16];
uniform float3 u_CameraPos;
uniform float3 u_DiffuseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_ReflectionStrength;
uniform int u_HasEnvironmentMap;
uniform samplerCUBE u_EnvironmentMap;

uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_ARMTexture;
uniform int u_HasDiffuseTexture;
uniform int u_HasNormalTexture;
uniform int u_HasARMTexture;

float3 calculateNormal(FragmentInput input) {
    if (u_HasNormalTexture) {
        float3 T = normalize(input.tangent);
        float3 N = normalize(input.normal);
        float3 B = normalize(input.bitangent);
        float bLen = length(input.bitangent);
        if (bLen < 0.01f) {
            return N;
        }
        float3 normalMap = normalize(tex2D(u_NormalTexture, input.texCoord).rgb * 2.0f - 1.0f);
        float3x3 TBN = float3x3(T, B, N);
        return normalize(mul(normalMap, TBN));
    } else {
        return normalize(input.normal);
    }
}

float3 calculateDirectionalLight(Light light, float3 normal, float3 viewDir) {
    float3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = light.color.rgb * light.direction.w * diff;
    return diffuse;
}

float3 calculatePointLight(Light light, float3 normal, float3 viewDir, float3 worldPos) {
    float3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(lightDir, normal), 0.0f);
    
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    float3 diffuse = light.color.rgb * light.direction.w * diff * attenuation;
    return diffuse;
}

float3 calculateSpotLight(Light light, float3 normal, float3 viewDir, float3 worldPos) {
    float3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(lightDir, normal), 0.0f);
    
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.x - light.params.y;
    float intensity = clamp((theta - light.params.y) / epsilon, 0.0f, 1.0f);
    
    float3 diffuse = light.color.rgb * light.direction.w * diff * attenuation * intensity;
    return diffuse;
}

float4 main(FragmentInput input) : COLOR {
    float3 diffuseColor = u_DiffuseColor;
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    float ambientOcclusion = 1.0f;
    
    if (u_HasDiffuseTexture) {
        float4 diffuseSample = tex2D(u_DiffuseTexture, input.texCoord);
        diffuseColor = diffuseSample.rgb;
    }
    
    if (u_HasARMTexture) {
        float4 armSample = tex2D(u_ARMTexture, input.texCoord);
        ambientOcclusion = armSample.r;
        roughness = armSample.g;
        metallic = armSample.b;
    }
    
    float3 normal = calculateNormal(input);
    float3 viewDir = normalize(u_CameraPos - input.worldPos);
    
    float3 result = float3(0.0f, 0.0f, 0.0f);
    
    for (int i = 0; i < u_NumLights; i++) {
        Light light = u_Lights[i];
        float lightType = light.position.w;
        
        if (lightType == 0.0f) {
            result += calculateDirectionalLight(light, normal, viewDir);
        } else if (lightType == 1.0f) {
            result += calculatePointLight(light, normal, viewDir, input.worldPos);
        } else if (lightType == 2.0f) {
            result += calculateSpotLight(light, normal, viewDir, input.worldPos);
        }
    }
    
    float3 ambient = float3(0.1f, 0.1f, 0.1f) * diffuseColor * ambientOcclusion;
    result = ambient + result * diffuseColor;

    if (u_HasEnvironmentMap && u_ReflectionStrength > 0.0f) {
        float3 I = normalize(input.worldPos - u_CameraPos);
        float3 R = reflect(I, normalize(normal));
        float3 env = texCUBE(u_EnvironmentMap, R).rgb;
        result = lerp(result, env, u_ReflectionStrength);
    }
    
    return float4(result, 1.0f);
})";
    } else {
        return R"(#version 120
// GLSL Fragment Shader
// Custom Lit Shader

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 attenuation;
};

uniform int u_NumLights;
uniform Light u_Lights[16];
uniform vec3 u_CameraPos;
uniform vec3 u_DiffuseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_ReflectionStrength;
uniform bool u_HasEnvironmentMap;
uniform samplerCube u_EnvironmentMap;

uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_ARMTexture;
uniform bool u_HasDiffuseTexture;
uniform bool u_HasNormalTexture;
uniform bool u_HasARMTexture;

varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;
varying vec3 vViewPos;
varying vec3 vTangent;
varying vec3 vBitangent;

vec3 calculateNormal() {
    if (u_HasNormalTexture) {
        vec3 T = normalize(vTangent);
        vec3 N = normalize(vNormal);
        vec3 B = normalize(vBitangent);
        float bLen = length(vBitangent);
        if (bLen < 0.01) {
            return N;
        }
        vec3 normalMap = normalize(texture2D(u_NormalTexture, vTexCoord).rgb * 2.0 - 1.0);
        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * normalMap);
    } else {
        return normalize(vNormal);
    }
}

vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = light.color.rgb * light.direction.w * diff;
    return diffuse;
}

vec3 calculatePointLight(Light light, vec3 normal, vec3 viewDir, vec3 worldPos) {
    vec3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(lightDir, normal), 0.0);
    
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return vec3(0.0);
    
    float attenuation = 1.0 / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    vec3 diffuse = light.color.rgb * light.direction.w * diff * attenuation;
    return diffuse;
}

vec3 calculateSpotLight(Light light, vec3 normal, vec3 viewDir, vec3 worldPos) {
    vec3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(lightDir, normal), 0.0);
    
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return vec3(0.0);
    
    float attenuation = 1.0 / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.x - light.params.y;
    float intensity = clamp((theta - light.params.y) / epsilon, 0.0, 1.0);
    
    vec3 diffuse = light.color.rgb * light.direction.w * diff * attenuation * intensity;
    return diffuse;
}

void main() {
    vec3 diffuseColor = u_DiffuseColor;
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    float ambientOcclusion = 1.0;
    
    if (u_HasDiffuseTexture) {
        diffuseColor = texture2D(u_DiffuseTexture, vTexCoord).rgb;
    }
    
    if (u_HasARMTexture) {
        vec3 arm = texture2D(u_ARMTexture, vTexCoord).rgb;
        ambientOcclusion = arm.r;
        roughness = arm.g;
        metallic = arm.b;
    }
    
    vec3 normal = calculateNormal();
    vec3 viewDir = normalize(u_CameraPos - vWorldPos);
    
    vec3 result = vec3(0.0);
    
    for (int i = 0; i < u_NumLights; i++) {
        Light light = u_Lights[i];
        float lightType = light.position.w;
        
        if (lightType == 0.0) {
            result += calculateDirectionalLight(light, normal, viewDir);
        } else if (lightType == 1.0) {
            result += calculatePointLight(light, normal, viewDir, vWorldPos);
        } else if (lightType == 2.0) {
            result += calculateSpotLight(light, normal, viewDir, vWorldPos);
        }
    }
    
    vec3 ambient = vec3(0.1) * diffuseColor * ambientOcclusion;
    result = ambient + result * diffuseColor;

    if (u_HasEnvironmentMap && u_ReflectionStrength > 0.0) {
        vec3 I = normalize(vWorldPos - u_CameraPos);
        vec3 R = reflect(I, normalize(normal));
        vec3 env = textureCube(u_EnvironmentMap, R).rgb;
        result = mix(result, env, u_ReflectionStrength);
    }
    
    gl_FragColor = vec4(result, 1.0);
})";
    }
}

std::string ShaderManager::generateUnlitVertexShader(bool isVita) {
    if (isVita) {
        return R"(// CG Vertex Shader
// Custom Unlit Shader

struct VertexInput {
    float3 aPosition : POSITION;
    float3 aNormal   : NORMAL;
    float2 aTexCoord : TEXCOORD0;
    float3 aTangent  : TEXCOORD1;
    float4 boneWeights : TEXCOORD2;
    float4 boneIndices : TEXCOORD3;
};

struct VertexOutput {
    float4 position  : POSITION;
    float2 texCoord  : TEXCOORD0;
};

uniform float4x4 modelMatrix;
uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;

uniform float4x4 u_BoneMatrices[100];
uniform int u_NumBones;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    float4 skinnedPosition = float4(input.aPosition, 1.0f);

    if (u_NumBones > 0) {
        int boneIndex0 = (int)input.boneIndices.x;
        int boneIndex1 = (int)input.boneIndices.y;
        int boneIndex2 = (int)input.boneIndices.z;
        int boneIndex3 = (int)input.boneIndices.w;
        
        int maxBoneIndex = u_NumBones - 1;
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        float4x4 boneTransform = 
            u_BoneMatrices[boneIndex0] * input.boneWeights.x +
            u_BoneMatrices[boneIndex1] * input.boneWeights.y +
            u_BoneMatrices[boneIndex2] * input.boneWeights.z +
            u_BoneMatrices[boneIndex3] * input.boneWeights.w;
        
        skinnedPosition = mul(float4(input.aPosition, 1.0f), boneTransform);
    }
    
    float4 worldPos = mul(skinnedPosition, modelMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    
    output.texCoord = input.aTexCoord;
    output.position = mul(viewPos, projectionMatrix);
    
    return output;
})";
    } else {
        return R"(#version 120
// GLSL Vertex Shader
// Custom Unlit Shader

attribute vec3 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;
attribute vec4 boneWeights;
attribute vec4 boneIndices;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform mat4 u_BoneMatrices[100];
uniform int u_NumBones;

varying vec2 vTexCoord;

void main() {
    vec4 skinnedPosition = vec4(position, 1.0);
    
    if (u_NumBones > 0 && boneWeights.x > 0.0) {
        ivec4 boneIndicesInt = ivec4(floor(boneIndices + 0.5));
        int boneIndex0 = boneIndicesInt.x;
        int boneIndex1 = boneIndicesInt.y;
        int boneIndex2 = boneIndicesInt.z;
        int boneIndex3 = boneIndicesInt.w;
        
        int maxBoneIndex = u_NumBones - 1;
        if (maxBoneIndex > 99) maxBoneIndex = 99;
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        vec4 pos0 = u_BoneMatrices[boneIndex0] * vec4(position, 1.0);
        vec4 pos1 = u_BoneMatrices[boneIndex1] * vec4(position, 1.0);
        vec4 pos2 = u_BoneMatrices[boneIndex2] * vec4(position, 1.0);
        vec4 pos3 = u_BoneMatrices[boneIndex3] * vec4(position, 1.0);
        
        skinnedPosition = pos0 * boneWeights.x + pos1 * boneWeights.y + pos2 * boneWeights.z + pos3 * boneWeights.w;
    }
    
    vec4 worldPos = modelMatrix * skinnedPosition;
    vec4 viewPos = viewMatrix * worldPos;
    
    vTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);
    gl_Position = projectionMatrix * viewPos;
})";
    }
}

std::string ShaderManager::generateUnlitFragmentShader(bool isVita) {
    if (isVita) {
        return R"(// CG Fragment Shader
// Custom Unlit Shader

struct FragmentInput {
    float2 texCoord : TEXCOORD0;
};

uniform float3 u_DiffuseColor;
uniform sampler2D u_DiffuseTexture;
uniform int u_HasDiffuseTexture;

float4 main(FragmentInput input) : COLOR {
    float3 color = u_DiffuseColor;
    
    if (u_HasDiffuseTexture) {
        float4 texSample = tex2D(u_DiffuseTexture, input.texCoord);
        color = texSample.rgb;
    }
    
    return float4(color, 1.0f);
})";
    } else {
        return R"(#version 120
// GLSL Fragment Shader
// Custom Unlit Shader

varying vec2 vTexCoord;

uniform vec3 u_DiffuseColor;
uniform sampler2D u_DiffuseTexture;
uniform bool u_HasDiffuseTexture;

void main() {
    vec3 color = u_DiffuseColor;
    
    if (u_HasDiffuseTexture) {
        color = texture2D(u_DiffuseTexture, vTexCoord).rgb;
    }
    
    gl_FragColor = vec4(color, 1.0);
})";
    }
}

bool ShaderManager::createShader(const std::string& shaderName, ShaderType type, const std::string& platform, std::string& outVertexPath, std::string& outFragmentPath) {
    std::string shaderDir = getShaderDirectory(platform);
    
    std::string vertExt = ".vert";
    std::string fragExt = ".frag";
    
    outVertexPath = shaderDir + "/" + shaderName + vertExt;
    outFragmentPath = shaderDir + "/" + shaderName + fragExt;
    
#ifdef LINUX_BUILD
    if (std::filesystem::exists(outVertexPath) || std::filesystem::exists(outFragmentPath)) {
        std::cerr << "Shader files already exist: " << shaderName << " in " << platform << " directory" << std::endl;
        return false;
    }
#else
    std::ifstream testFile(outVertexPath);
    if (testFile.good()) {
        testFile.close();
        std::cerr << "Shader files already exist: " << shaderName << " in " << platform << " directory" << std::endl;
        return false;
    }
    testFile.close();
    testFile.open(outFragmentPath);
    if (testFile.good()) {
        testFile.close();
        std::cerr << "Shader files already exist: " << shaderName << " in " << platform << " directory" << std::endl;
        return false;
    }
    testFile.close();
#endif
    
    bool isVita = (platform == "vita" || platform == "Vita" || platform == "VITA");
#ifdef VITA_BUILD
    if (platform.empty()) {
        isVita = true;
    }
#endif
    
    std::string vertexSource, fragmentSource;
    
    if (type == ShaderType::Lit) {
        vertexSource = generateLitVertexShader(isVita);
        fragmentSource = generateLitFragmentShader(isVita);
    } else {
        vertexSource = generateUnlitVertexShader(isVita);
        fragmentSource = generateUnlitFragmentShader(isVita);
    }
    
    if (!writeShaderFile(outVertexPath, vertexSource)) {
        return false;
    }
    
    if (!writeShaderFile(outFragmentPath, fragmentSource)) {
#ifdef LINUX_BUILD
        std::filesystem::remove(outVertexPath);
#else
        std::remove(outVertexPath.c_str());
#endif
        return false;
    }
    
    std::cout << "Created " << platform << " shader: " << shaderName << " (" << (type == ShaderType::Lit ? "Lit" : "Unlit") << ")" << std::endl;
    return true;
}

std::shared_ptr<Shader> ShaderManager::loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string cacheKey = getShaderCacheKey(vertexPath, fragmentPath);
    
    auto it = shaderCache.find(cacheKey);
    if (it != shaderCache.end()) {
        return it->second;
    }
    
    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(vertexPath, fragmentPath)) {
        std::cerr << "Failed to load shader: " << vertexPath << ", " << fragmentPath << std::endl;
        return nullptr;
    }
    
    shaderCache[cacheKey] = shader;
    
    return shader;
}

std::shared_ptr<Shader> ShaderManager::getShader(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string cacheKey = getShaderCacheKey(vertexPath, fragmentPath);
    auto it = shaderCache.find(cacheKey);
    if (it != shaderCache.end()) {
        return it->second;
    }
    return nullptr;
}

bool ShaderManager::isLitShader(std::shared_ptr<Shader> shader) const {
    if (!shader) return false;
    
    if (shader == Shader::getLightingShader()) {
        return true;
    }
    
    for (const auto& pair : shaderCache) {
        if (pair.second == shader) {
            auto typeIt = shaderTypes.find(pair.first);
            if (typeIt != shaderTypes.end()) {
                return typeIt->second == ShaderType::Lit;
            }
        }
    }
    
    return true;
}

void ShaderManager::registerShaderType(std::shared_ptr<Shader> shader, ShaderType type) {
    if (!shader) return;
    
    for (const auto& pair : shaderCache) {
        if (pair.second == shader) {
            shaderTypes[pair.first] = type;
            return;
        }
    }
}

} // namespace GameEngine
