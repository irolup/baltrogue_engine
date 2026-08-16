#version 120

// Light structure
struct Light {
    vec4 position;      // w component: 0 = directional, 1 = point, 2 = spot
    vec4 direction;     // w component: intensity
    vec4 color;         // w component: range
    vec4 params;        // x: cutOff, y: outerCutOff, z: constant, w: linear
    vec4 attenuation;   // x: quadratic, y: unused, z: unused, w: unused
};

// Uniforms
uniform int u_NumLights;
uniform Light u_Lights[16];
uniform vec3 u_CameraPos;

const vec2 ATLAS_TILES = vec2(4.0, 2.0);
uniform sampler2D u_ShadowMap;
uniform mat4 u_ShadowMatrices[8];
uniform int u_NumShadowViews;
uniform int u_ReceiveShadows;
uniform vec4 u_ShadowParams; // x = 1/atlasWidth, y = 1/atlasHeight, z = soft filter

uniform vec3 u_DiffuseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_ReflectionStrength;
uniform float u_Opacity;
uniform float u_AlphaCutoff;
uniform bool u_HasEnvironmentMap;
uniform samplerCube u_EnvironmentMap;

// Texture uniforms
uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_ARMTexture;
uniform bool u_HasDiffuseTexture;
uniform bool u_HasNormalTexture;
uniform bool u_HasARMTexture;

// Varyings (inputs from vertex shader)
varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;
varying vec3 vViewPos;
varying vec3 vTangent;
varying vec3 vBitangent;

// Helper function to convert normal from tangent space to world space
vec3 calculateNormal() {
    if (u_HasNormalTexture) {
        vec3 T = normalize(vTangent);
        vec3 N = normalize(vNormal);
        vec3 B = normalize(vBitangent);
        float bLen = length(vBitangent);
        if (bLen < 0.01) {
            return N;
        }
        // Sample normal from texture and convert from [0,1] to [-1,1]
        vec3 normalMap = normalize(texture2D(u_NormalTexture, vTexCoord).rgb * 2.0 - 1.0);
        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * normalMap);
    } else {
        return normalize(vNormal);
    }
}

// Helper function to get material properties from ARM texture
vec3 getMaterialProperties() {
    if (u_HasARMTexture) {
        vec3 arm = texture2D(u_ARMTexture, vTexCoord).rgb;
        return vec3(arm.r, arm.g, arm.b); // AO, Roughness, Metallic
    } else {
        return vec3(1.0, u_Roughness, u_Metallic); // Default values
    }
}

// Face order must match ShadowManager::addPointViews: +X -X +Y -Y +Z -Z.
int cubeFaceIndex(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return (d.x > 0.0) ? 0 : 1;
    if (a.y >= a.z) return (d.y > 0.0) ? 2 : 3;
    return (d.z > 0.0) ? 4 : 5;
}

// 1.0 = fully lit, 0.0 = fully occluded.
float sampleShadowView(int view, vec3 worldPos, float bias) {
    vec4 lightClip = u_ShadowMatrices[view] * vec4(worldPos, 1.0);
    if (lightClip.w <= 0.0) return 1.0;

    vec3 proj = lightClip.xyz / lightClip.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.z < 0.0) return 1.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;

    // Stay half a texel inside the tile so filtering cannot read a neighbour.
    vec2 tileTexel = u_ShadowParams.xy * ATLAS_TILES;
    vec2 tileUV = clamp(proj.xy, tileTexel * 0.5, vec2(1.0) - tileTexel * 0.5);
    vec2 tileOrigin = vec2(mod(float(view), ATLAS_TILES.x), floor(float(view) / ATLAS_TILES.x));
    vec2 atlasUV = (tileOrigin + tileUV) / ATLAS_TILES;

    float compare = proj.z - bias;
    float lit = step(compare, texture2D(u_ShadowMap, atlasUV).r);

    if (u_ShadowParams.z > 0.5) {
        lit += step(compare, texture2D(u_ShadowMap, atlasUV + vec2(u_ShadowParams.x, 0.0)).r);
        lit += step(compare, texture2D(u_ShadowMap, atlasUV + vec2(0.0, u_ShadowParams.y)).r);
        lit += step(compare, texture2D(u_ShadowMap, atlasUV + u_ShadowParams.xy).r);
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

void main() {
    // Get material properties
    vec3 materialProps = getMaterialProperties();
    float ao = materialProps.r;
    float roughness = materialProps.g;
    float metallic = materialProps.b;
    
    // Get normal (with normal mapping if available)
    vec3 normal = calculateNormal();
    vec3 viewDir = normalize(u_CameraPos - vWorldPos);
    
    // Get diffuse color / alpha
    vec3 diffuseColor = u_DiffuseColor;
    float alpha = u_Opacity;
    if (u_HasDiffuseTexture) {
        vec4 diffuseSample = texture2D(u_DiffuseTexture, vTexCoord);
        diffuseColor *= diffuseSample.rgb;
        alpha *= diffuseSample.a;
    }

    if (u_AlphaCutoff > 0.0 && alpha < u_AlphaCutoff) {
        discard;
    }
    
    // Initialize lighting result
    vec3 result = vec3(0.0);

    bool shadowsActive = (u_NumShadowViews > 0) && (u_ReceiveShadows != 0);

    // Calculate lighting from all lights
    for (int i = 0; i < u_NumLights; i++) {
        Light light = u_Lights[i];
        float lightType = light.position.w;

        vec3 contribution = vec3(0.0);
        if (lightType == 0.0) {
            // Directional light
            contribution = calculateDirectionalLight(light, normal, viewDir);
        } else if (lightType == 1.0) {
            // Point light
            contribution = calculatePointLight(light, normal, viewDir, vWorldPos);
        } else if (lightType == 2.0) {
            // Spot light
            contribution = calculateSpotLight(light, normal, viewDir, vWorldPos);
        }

        if (shadowsActive && dot(contribution, contribution) > 0.0) {
            contribution *= computeShadow(light, normal, vWorldPos);
        }

        result += contribution;
    }
    
    // Apply material properties
    vec3 ambient = vec3(0.3) * diffuseColor * ao; // Ambient lighting with AO
    result = ambient + result * diffuseColor;

    if (u_HasEnvironmentMap && u_ReflectionStrength > 0.0) {
        vec3 I = normalize(vWorldPos - u_CameraPos);
        vec3 R = reflect(I, normalize(normal));
        vec3 env = textureCube(u_EnvironmentMap, R).rgb;
        // Single control: 0 = no reflection, 1 = full mirror
        result = mix(result, env, u_ReflectionStrength);
    }
    
    // Output final color (alpha drives Alpha / Additive blend modes)
    gl_FragColor = vec4(result, alpha);
}
