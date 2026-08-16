#version 450

const float PI = 3.1415926535897932384626433832795;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in vec3 inWorldTangent;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 attenuation;
};

layout(std140, set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    int numLights;
    int hasEnvironmentMap;
    int numShadowViews;
    int _pad2;
    Light lights[16];
    vec4 shadowParams; // x = 1/atlasWidth, y = 1/atlasHeight, z = soft filter
    mat4 shadowMatrices[8];
} uFrame;

// Must match kShadowAtlasCols / kShadowAtlasRows in ShadowMap.h.
const vec2 ATLAS_TILES = vec2(4.0, 2.0);
layout(set = 0, binding = 1) uniform sampler2D uShadowMap;

layout(set = 1, binding = 0) uniform sampler2D uDiffuseTexture;
layout(set = 1, binding = 1) uniform sampler2D uNormalTexture;
layout(set = 1, binding = 2) uniform sampler2D uARMTexture;

layout(std140, set = 1, binding = 3) uniform MaterialUniforms {
    vec4 baseColor;
    float roughness;
    float metallic;
    float reflectionStrength;
    float alphaCutoff;
    vec4 textureFlags;
    vec4 uvScaleOffset; // xy = scale, zw = offset
} uMaterial;

layout(set = 2, binding = 0) uniform samplerCube uEnvironmentMap;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int objectID;
    int receiveShadows;
    int shadowViewIndex;
    int _pad2;
} uPushConstants;

layout(location = 0) out vec4 outColor;

//ON devrait remplacer par :
// struct Material {
//     vec4 baseColor;

//     float metallic;
//     float roughness;
//     float ao;

//     float emissiveStrength;
// };

//et les textures:
// uniform sampler2D u_DiffuseTexture;
// uniform sampler2D u_NormalTexture;
// uniform sampler2D u_ARMTexture;


// vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
// { 
//     const float minLayers = 8;
//     const float maxLayers = 32;
//     float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 0.0), viewDir)));
//     float layerDepth = 1.0 / numLayers;
//     float currentLayerDepth = 0.0;
//     vec2 P = viewDir.xy * heightScale; 
//     vec2 deltaTexCoords = P / numLayers;
//     vec2 currentTexCoords = texCoords;
//     float currentDepthMapValue = texture(texture_disp, currentTexCoords).r;

//     while(currentLayerDepth < currentDepthMapValue)
//     {
//         currentTexCoords -= deltaTexCoords;
//         currentDepthMapValue = texture(texture_disp, currentTexCoords).r;
//         currentLayerDepth += layerDepth;  
//     }

//     vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
//     float afterDepth  = currentDepthMapValue - currentLayerDepth;
//     float beforeDepth = texture(texture_disp, prevTexCoords).r - currentLayerDepth + layerDepth;

//     float weight = afterDepth / (afterDepth - beforeDepth);
//     vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

//     return finalTexCoords;
// }



// mat3 CotangentFrame(in vec3 N, in vec3 p, in vec2 uv) {
//   vec3 dp1 = dFdx(p);
//   vec3 dp2 = dFdy(p);
//   vec2 duv1 = dFdx(uv);
//   vec2 duv2 = dFdy(uv);

//   vec3 dp2perp = cross(dp2, N);
//   vec3 dp1perp = cross(N, dp1);
//   vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
//   vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

//   float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
//   return mat3(T * invmax, B * invmax, N);
// }

// vec3 getNormalFromMap(vec2 texcoord){
//     vec3 tangentNormal = texture(texture_normal, texcoord).xyz * 2.0 - 1.0;

//     vec3 T = normalize(Tangent - dot(Tangent, Normal) * Normal);
//     vec3 B = normalize(cross(Normal, T));
//     mat3 TBN = mat3(T, B, Normal);

//     return normalize(TBN * tangentNormal);
// }


// // Perturb normal using normal map
// vec3 perturb_normal( vec3 N, vec3 V, vec2 texcoord )
// {
//     // assume N, the interpolated vertex normal and
//     // V, the view vector (vertex to eye)
//     vec3 map = texture(texture_normal, texcoord ).xyz;
//     map = map * 2.0 - 1.0;
//     mat3 TBN = CotangentFrame(N, -V, texcoord);
//     return normalize(TBN * map);
// }

// // Normal Distribution Function (NDF) - Trowbridge-Reitz GGX
// float trowbridge_reitz(vec3 N, vec3 H, float roughness)
// {
//     float a = roughness * roughness;
//     float NdotH = max(dot(N, H), 0.0);
//     float denom = (NdotH * NdotH * (a - 1.0) + 1.0);
//     return (a * a) / (PI * denom * denom + 0.0001); // Avoid division by zero
// }

// // Geometry Function (Smith's method)
// float smith(vec3 N, vec3 L, vec3 V, float roughness)
// {
//     float NdotL = max(dot(N, L), 0.0);
//     float NdotV = max(dot(N, V), 0.0);
//     float ggx2 = schlick_beckmann(NdotV, roughness);
//     float ggx1 = schlick_beckmann(NdotL, roughness);
//     return ggx1 * ggx2;
// }

// // Schlick-Beckmann approximation for geometry function
// float schlick_beckmann(float NdotX, float roughness)
// {
//     float k = (roughness + 1.0);
//     k = (k * k) / 8.0;
//     return NdotX / (NdotX * (1.0 - k) + k + 0.0001); // Avoid division by zero
// }

// // Fresnel Equation (Schlick's approximation)
// vec3 schlick_fresnel(float cosTheta, vec3 F0)
// {
//     return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
// }


// // ACES Filmic tone mapping
// vec3 tone_mapping_aces_filmic(vec3 color)
// {
//     float a = 2.51;
//     float b = 0.03;
//     float c = 2.43;
//     float d = 0.59;
//     float e = 0.14;
//     return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
// }

// Face order must match ShadowManager::addPointViews: +X -X +Y -Y +Z -Z.
int cubeFaceIndex(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return (d.x > 0.0) ? 0 : 1;
    if (a.y >= a.z) return (d.y > 0.0) ? 2 : 3;
    return (d.z > 0.0) ? 4 : 5;
}

// 1.0 = fully lit, 0.0 = fully occluded.
float sampleShadowView(int view, vec3 worldPos, float bias) {
    vec4 lightClip = uFrame.shadowMatrices[view] * vec4(worldPos, 1.0);
    if (lightClip.w <= 0.0) return 1.0;

    vec3 proj = lightClip.xyz / lightClip.w;
    // Vulkan clip space already puts z in [0,1]; only xy needs remapping.
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.z < 0.0) return 1.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;

    vec2 tileTexel = uFrame.shadowParams.xy * ATLAS_TILES;
    vec2 tileUV = clamp(proj.xy, tileTexel * 0.5, vec2(1.0) - tileTexel * 0.5);
    vec2 tileOrigin = vec2(mod(float(view), ATLAS_TILES.x), floor(float(view) / ATLAS_TILES.x));
    vec2 atlasUV = (tileOrigin + tileUV) / ATLAS_TILES;

    float compare = proj.z - bias;
    float lit = step(compare, texture(uShadowMap, atlasUV).r);

    if (uFrame.shadowParams.z > 0.5) {
        lit += step(compare, texture(uShadowMap, atlasUV + vec2(uFrame.shadowParams.x, 0.0)).r);
        lit += step(compare, texture(uShadowMap, atlasUV + vec2(0.0, uFrame.shadowParams.y)).r);
        lit += step(compare, texture(uShadowMap, atlasUV + uFrame.shadowParams.xy).r);
        lit *= 0.25;
    }

    return lit;
}

float computeShadow(Light light, vec3 normal, vec3 worldPos) {
    int baseView = int(light.attenuation.y);
    if (baseView < 0) return 1.0;

    vec3 lightDir = (light.position.w < 0.5)
        ? normalize(-light.direction.xyz)
        : normalize(light.position.xyz - worldPos);

    int view = baseView;
    if (light.position.w > 0.5 && light.position.w < 1.5) {
        view = baseView + cubeFaceIndex(worldPos - light.position.xyz);
    }

    float ndotl = max(dot(normal, lightDir), 0.0);
    float bias = light.attenuation.w * max(1.0, 3.0 * (1.0 - ndotl));

    return mix(1.0, sampleShadowView(view, worldPos, bias), light.attenuation.z);
}

// Lighting calculation functions
vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.color.rgb * light.direction.w * diff;
    return diffuse;
}

vec3 calculatePointLight(Light light, vec3 normal, vec3 viewDir, vec3 worldPos) {
    vec3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Calculate distance and attenuation
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return vec3(0.0); // Beyond range
    
    float attenuation = 1.0 / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    
    vec3 diffuse = light.color.rgb * light.direction.w * diff * attenuation;
    return diffuse;
}

vec3 calculateSpotLight(Light light, vec3 normal, vec3 viewDir, vec3 worldPos) {
    vec3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Calculate distance and attenuation
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return vec3(0.0); // Beyond range
    
    float attenuation = 1.0 / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    
    // Calculate spot light cone
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.x - light.params.y;
    float intensity = clamp((theta - light.params.y) / epsilon, 0.0, 1.0);
    
    vec3 diffuse = light.color.rgb * light.direction.w * diff * attenuation * intensity;
    return diffuse;
}

// // Calculate PBR lighting using Cook-Torrance BRDF
// vec3 CalculateLightingPBR(Light light, vec3 N, vec3 V, vec3 fragPos, vec3 albedo, float metallic, float roughness, float ao)
// {


//     vec3 ambient = material.ambient * albedo * ao; 

//     // Fresnel reflectance at normal incidence
//     mix(vec3(0.04), albedo, uMaterial.metallic);

//     F0 = mix(F0, albedo, metallic);
//     //F0 = mix(vec3(0.04), albedo, metallic);

//     // Light direction
//     vec3 L;
//     if (light.type == 2) // Directional light
//     {
//         L = normalize(-light.direction);
//     }
//     else // Point light or Spotlight
//     {
//         L = normalize(light.position - fragPos);
//     }

//     // Half vector
//     vec3 H = normalize(V + L);

//     // NDF
//     float NDF = trowbridge_reitz(N, H, roughness);

//     // Geometry
//     float G = smith(N, L, V, roughness);

//     // Fresnel
//     vec3 F = schlick_fresnel(max(dot(H, V), 0.0), F0);

//     // Specular reflection
//     vec3 numerator = NDF * G * F;
//     float NdotV = max(dot(N, V), 0.0);
//     float NdotL = max(dot(N, L), 0.0);
//     float denominator = 4.0 * NdotV * NdotL + 0.0001; // Avoid division by zero
//     vec3 specular = numerator / denominator;

//     specular = specular * material.specular;

//     // Diffuse reflection
//     vec3 kD = vec3(1.0) - F; // Energy conservation
//     kD *= 1.0 - metallic;

//     // Light radiance
//     vec3 radiance;
//     if (light.type == 2) // Directional light have pos and dir
//     {
//         // Ensure light direction is normalized
//         vec3 lightDir = normalize( light.position - fragPos );
//         float NdotL = max(dot(N, lightDir), 0.0);    // Angle between normal and light direction
    
//         // Radiance depends on light color, intensity, and NdotL
//         radiance = light.color.rgb * light.intensity * NdotL;
//     }
//     else // Point light or Spotlight
//     {
//         float distance = length(light.position - fragPos);
//         float attenuation = 1.0 / (distance * distance); // Simple attenuation
//         radiance = light.color.rgb * attenuation * light.intensity *50.0;
//     }

//     // Final reflected light
//     vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

//     vec3 color = ( ambient + Lo ) * material.brightness;

//     return color;
// }



void main() {
    vec2 uv = inTexCoord * uMaterial.uvScaleOffset.xy + uMaterial.uvScaleOffset.zw;

    vec3 albedo = uMaterial.baseColor.rgb;
    float alpha = uMaterial.baseColor.a;
    if (uMaterial.textureFlags.x > 0.5) {
        vec4 diffuseSample = texture(uDiffuseTexture, uv);
        albedo *= diffuseSample.rgb;
        alpha *= diffuseSample.a;
    }

    // glTF MASK: hard cutout (leaves, fences, etc.)
    if (uMaterial.alphaCutoff > 0.0 && alpha < uMaterial.alphaCutoff) {
        discard;
    }

    vec3 normal = normalize(inNormal);
    if (uMaterial.textureFlags.y > 0.5) {
        vec3 tangent = normalize(inWorldTangent - dot(inWorldTangent, normal) * normal);
        vec3 bitangent = normalize(cross(normal, tangent));
        mat3 tbn = mat3(tangent, bitangent, normal);
        vec3 normalSample = texture(uNormalTexture, uv).xyz * 2.0 - 1.0;
        normal = normalize(tbn * normalSample);
    }

    vec3 viewDir = normalize(uFrame.cameraPosition.xyz - inWorldPos);
    vec3 result = vec3(0.0);

    bool shadowsActive = (uFrame.numShadowViews > 0) && (uPushConstants.receiveShadows != 0);

    for (int i = 0; i < uFrame.numLights; i++) {
        Light light = uFrame.lights[i];
        float lightType = light.position.w;

        vec3 contribution = vec3(0.0);
        if (lightType == 0.0)
            contribution = calculateDirectionalLight(light, normal, viewDir);
        else if (lightType == 1.0)
            contribution = calculatePointLight(light, normal, viewDir, inWorldPos);
        else if (lightType == 2.0)
            contribution = calculateSpotLight(light, normal, viewDir, inWorldPos);

        if (shadowsActive && dot(contribution, contribution) > 0.0) {
            contribution *= computeShadow(light, normal, inWorldPos);
        }

        result += contribution;
    }
    float ao = 1.0;
    if (uMaterial.textureFlags.z > 0.5)
        ao = texture(uARMTexture, uv).r;
    vec3 ambient = vec3(0.3) * albedo * ao;
    vec3 color = ambient + result * albedo;
    //vec3 color = result * albedo;

    if (uFrame.hasEnvironmentMap != 0 && uMaterial.reflectionStrength > 0.0) {
        vec3 I = normalize(inWorldPos - uFrame.cameraPosition.xyz);
        vec3 R = reflect(I, normal);
        vec3 environmentColor = texture(uEnvironmentMap, R).rgb;
        color = mix(color, environmentColor, clamp(uMaterial.reflectionStrength, 0.0, 1.0));
    }

    outColor = vec4(color, alpha);
}