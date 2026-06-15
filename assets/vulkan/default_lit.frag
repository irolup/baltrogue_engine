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
    int _pad1, _pad2;
    Light lights[16];
} uFrame;

layout(set = 1, binding = 0) uniform sampler2D uDiffuseTexture;
layout(set = 1, binding = 1) uniform sampler2D uNormalTexture;
layout(set = 1, binding = 2) uniform sampler2D uARMTexture;

layout(std140, set = 1, binding = 3) uniform MaterialUniforms {
    vec4 baseColor;
    float roughness;
    float metallic;
    float reflectionStrength;
    float padding;
    vec4 textureFlags;
} uMaterial;

layout(set = 2, binding = 0) uniform samplerCube uEnvironmentMap;

layout(push_constant) uniform PushConstants {
    mat4 model;
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
    vec3 albedo = uMaterial.baseColor.rgb;
    if (uMaterial.textureFlags.x > 0.5) {
        albedo = texture(uDiffuseTexture, inTexCoord).rgb;
    }

    vec3 normal = normalize(inNormal);
    if (uMaterial.textureFlags.y > 0.5) {
        vec3 tangent = normalize(inWorldTangent - dot(inWorldTangent, normal) * normal);
        vec3 bitangent = normalize(cross(normal, tangent));
        mat3 tbn = mat3(tangent, bitangent, normal);
        vec3 normalSample = texture(uNormalTexture, inTexCoord).xyz * 2.0 - 1.0;
        normal = normalize(tbn * normalSample);
    }

    vec3 viewDir = normalize(uFrame.cameraPosition.xyz - inWorldPos);
    vec3 result = vec3(0.0);

    for (int i = 0; i < uFrame.numLights; i++) {
        Light light = uFrame.lights[i];
        float lightType = light.position.w;
        if (lightType == 0.0)
            result += calculateDirectionalLight(light, normal, viewDir);
        else if (lightType == 1.0)
            result += calculatePointLight(light, normal, viewDir, inWorldPos);
        else if (lightType == 2.0)
            result += calculateSpotLight(light, normal, viewDir, inWorldPos);
    }
    float ao = 1.0;
    if (uMaterial.textureFlags.z > 0.5)
        ao = texture(uARMTexture, inTexCoord).r;
    vec3 ambient = vec3(0.1) * albedo * ao;
    vec3 color = ambient + result * albedo;
    //vec3 color = result * albedo;

    if (uFrame.hasEnvironmentMap != 0 && uMaterial.reflectionStrength > 0.0) {
        vec3 I = normalize(inWorldPos - uFrame.cameraPosition.xyz);
        vec3 R = reflect(I, normal);
        vec3 environmentColor = texture(uEnvironmentMap, R).rgb;
        color = mix(color, environmentColor, clamp(uMaterial.reflectionStrength, 0.0, 1.0));
    }

    outColor = vec4(color, 1.0);
}