// CG Fragment Shader for PS Vita (VitaGL)
// Multi-light lighting system

struct FragmentInput {
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float3 viewPos  : TEXCOORD3;
    float3 tangent  : TEXCOORD4;
    float3 bitangent: TEXCOORD5;
};

// Light structure
struct Light {
    float4 position;      // w component: 0 = directional, 1 = point, 2 = spot
    float4 direction;     // w component: intensity
    float4 color;         // w component: range
    float4 params;        // x: cutOff, y: outerCutOff, z: constant, w: linear
    float4 attenuation;   // x: quadratic, y: unused, z: unused, w: unused
};

// Uniforms
uniform int u_NumLights;
uniform Light u_Lights[16];
uniform float3 u_CameraPos;
uniform sampler2D u_ShadowMap;
uniform float4x4 u_ShadowMatrices[8];
uniform int u_NumShadowViews;
uniform int u_ReceiveShadows;
uniform float4 u_ShadowParams; // x = 1/atlasWidth, y = 1/atlasHeight, z = soft filter
uniform float3 u_DiffuseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_ReflectionStrength;
uniform float u_Opacity;
uniform float u_AlphaCutoff;
uniform int u_HasEnvironmentMap;
uniform samplerCUBE u_EnvironmentMap;

// Texture samplers
uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_ARMTexture; // Ambient Occlusion, Roughness, Metallic
uniform int u_HasDiffuseTexture;
uniform int u_HasNormalTexture;
uniform int u_HasARMTexture;

// Helper function to convert normal from tangent space to world space
float3 calculateNormal(FragmentInput input) {
    if (u_HasNormalTexture) {
        float3 T = normalize(input.tangent);
        float3 N = normalize(input.normal);
        float3 B = normalize(input.bitangent);
        float bLen = length(input.bitangent);
        if (bLen < 0.01f) {
            return N;
        }
        // Sample normal from texture and convert from [0,1] to [-1,1]
        float3 normalMap = normalize(tex2D(u_NormalTexture, input.texCoord).rgb * 2.0f - 1.0f);
        float3x3 TBN = float3x3(T, B, N);
        return normalize(mul(normalMap, TBN));
    } else {
        return normalize(input.normal);
    }
}

// The atlas is an RGBA8 colour target because VitaGL cannot sample a depth
// attachment, so shadow_depth.frag packs 24 bits of depth across the channels.
// NOTE: 'packed' is a reserved type qualifier in Cg, so the encoded value must
// not be named that.
float unpackDepth(float4 encoded) {
    const float4 bitShift = float4(0.000000059604645f, 0.000015258789f, 0.00390625f, 1.0f);
    return dot(encoded, bitShift);
}

// Face order must match ShadowManager::addPointViews: +X -X +Y -Y +Z -Z.
int cubeFaceIndex(float3 d) {
    float3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return (d.x > 0.0f) ? 0 : 1;
    if (a.y >= a.z) return (d.y > 0.0f) ? 2 : 3;
    return (d.z > 0.0f) ? 4 : 5;
}

float4 projectIntoShadowView(int view, float4 worldPos) {
    if (view <= 0) return mul(worldPos, u_ShadowMatrices[0]);
    if (view == 1) return mul(worldPos, u_ShadowMatrices[1]);
    if (view == 2) return mul(worldPos, u_ShadowMatrices[2]);
    if (view == 3) return mul(worldPos, u_ShadowMatrices[3]);
    if (view == 4) return mul(worldPos, u_ShadowMatrices[4]);
    if (view == 5) return mul(worldPos, u_ShadowMatrices[5]);
    if (view == 6) return mul(worldPos, u_ShadowMatrices[6]);
    return mul(worldPos, u_ShadowMatrices[7]);
}

// 1.0 = fully lit, 0.0 = fully occluded.
float sampleShadowView(int view, float3 worldPos, float bias) {
    float4 lightClip = projectIntoShadowView(view, float4(worldPos, 1.0f));
    if (lightClip.w <= 0.0f) return 1.0f;

    float3 proj = lightClip.xyz / lightClip.w;
    proj = proj * 0.5f + 0.5f;
    if (proj.z > 1.0f || proj.z < 0.0f) return 1.0f;
    if (proj.x < 0.0f || proj.x > 1.0f || proj.y < 0.0f || proj.y > 1.0f) return 1.0f;

    // Stay half a texel inside the tile so filtering cannot read a neighbour.
    float2 tileCount = float2(4.0f, 2.0f);
    float2 tileTexel = u_ShadowParams.xy * tileCount;
    float2 tileUV = clamp(proj.xy, tileTexel * 0.5f, float2(1.0f, 1.0f) - tileTexel * 0.5f);
    float viewF = (float)view;
    float tileRow = floor(viewF * 0.25f);
    float2 tileOrigin = float2(viewF - tileRow * 4.0f, tileRow);
    float2 atlasUV = (tileOrigin + tileUV) / tileCount;

    float compareDepth = proj.z - bias;
    float litFactor = step(compareDepth, unpackDepth(tex2D(u_ShadowMap, atlasUV)));

    if (u_ShadowParams.z > 0.5f) {
        litFactor += step(compareDepth, unpackDepth(tex2D(u_ShadowMap, atlasUV + float2(u_ShadowParams.x, 0.0f))));
        litFactor += step(compareDepth, unpackDepth(tex2D(u_ShadowMap, atlasUV + float2(0.0f, u_ShadowParams.y))));
        litFactor += step(compareDepth, unpackDepth(tex2D(u_ShadowMap, atlasUV + u_ShadowParams.xy)));
        litFactor *= 0.25f;
    }

    return litFactor;
}

float computeShadow(Light light, float3 normal, float3 worldPos) {
    int baseView = (int)light.attenuation.y;
    if (baseView < 0) return 1.0f;

    float3 lightDir = (light.position.w < 0.5f)
        ? normalize(-light.direction.xyz)
        : normalize(light.position.xyz - worldPos);

    int view = baseView;
    if (light.position.w > 0.5f && light.position.w < 1.5f) {
        view = baseView + cubeFaceIndex(worldPos - light.position.xyz);
    }

    float ndotl = max(dot(normal, lightDir), 0.0f);
    float bias = light.attenuation.w * max(1.0f, 3.0f * (1.0f - ndotl));

    return lerp(1.0f, sampleShadowView(view, worldPos, bias), light.attenuation.z);
}

// Lighting calculation functions
float3 calculateDirectionalLight(Light light, float3 normal, float3 viewDir) {
    float3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = light.color.rgb * light.direction.w * diff;
    return diffuse;
}

float3 calculatePointLight(Light light, float3 normal, float3 viewDir, float3 worldPos) {
    float3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(lightDir, normal), 0.0f);
    
    // Calculate distance and attenuation
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return float3(0.0f, 0.0f, 0.0f); // Beyond range
    
    float attenuation = 1.0f / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    
    float3 diffuse = light.color.rgb * light.direction.w * diff * attenuation;
    return diffuse;
}

float3 calculateSpotLight(Light light, float3 normal, float3 viewDir, float3 worldPos) {
    float3 lightDir = normalize(light.position.xyz - worldPos);
    float diff = max(dot(lightDir, normal), 0.0f);
    
    // Calculate distance and attenuation
    float distance = length(light.position.xyz - worldPos);
    if (distance > light.color.w) return float3(0.0f, 0.0f, 0.0f); // Beyond range
    
    float attenuation = 1.0f / (light.params.z + light.params.w * distance + light.attenuation.x * distance * distance);
    
    // Calculate spot light cone
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.x - light.params.y;
    float intensity = clamp((theta - light.params.y) / epsilon, 0.0f, 1.0f);
    
    float3 diffuse = light.color.rgb * light.direction.w * diff * attenuation * intensity;
    return diffuse;
}

float4 main(FragmentInput input) : COLOR {
    // Sample textures
    float3 diffuseColor = u_DiffuseColor;
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    float ambientOcclusion = 1.0f;
    float alpha = u_Opacity;
    
    // Sample diffuse texture if available
    if (u_HasDiffuseTexture) {
        float4 diffuseSample = tex2D(u_DiffuseTexture, input.texCoord);
        diffuseColor = diffuseSample.rgb;
        alpha *= diffuseSample.a;
    }

    if (u_AlphaCutoff > 0.0f) {
        clip(alpha - u_AlphaCutoff);
    }
    
    // Sample ARM texture if available (Ambient Occlusion, Roughness, Metallic)
    if (u_HasARMTexture) {
        float4 armSample = tex2D(u_ARMTexture, input.texCoord);
        ambientOcclusion = armSample.r; // Red channel = Ambient Occlusion
        roughness = armSample.g;        // Green channel = Roughness
        metallic = armSample.b;         // Blue channel = Metallic
    }
    
    // Get normal (with proper normal mapping if available)
    float3 normal = calculateNormal(input);
    
    // Normalize inputs
    float3 viewDir = normalize(u_CameraPos - input.worldPos);
    
    // Initialize lighting result
    float3 result = float3(0.0f, 0.0f, 0.0f);

    bool shadowsActive = (u_NumShadowViews > 0) && (u_ReceiveShadows != 0);

    // Calculate lighting from all lights
    for (int i = 0; i < u_NumLights; i++) {
        Light light = u_Lights[i];
        float lightType = light.position.w;

        float3 contribution = float3(0.0f, 0.0f, 0.0f);
        if (lightType == 0.0f) {
            // Directional light
            contribution = calculateDirectionalLight(light, normal, viewDir);
        } else if (lightType == 1.0f) {
            // Point light
            contribution = calculatePointLight(light, normal, viewDir, input.worldPos);
        } else if (lightType == 2.0f) {
            // Spot light
            contribution = calculateSpotLight(light, normal, viewDir, input.worldPos);
        }

        if (shadowsActive && dot(contribution, contribution) > 0.0f) {
            contribution *= computeShadow(light, normal, input.worldPos);
        }

        result += contribution;
    }
    
    // Apply material properties with textures
    float3 ambient = float3(0.3f, 0.3f, 0.3f) * diffuseColor * ambientOcclusion; // Ambient lighting with AO
    result = ambient + result * diffuseColor;

    if (u_HasEnvironmentMap && u_ReflectionStrength > 0.0f) {
        float3 I = normalize(input.worldPos - u_CameraPos);
        float3 R = reflect(I, normalize(normal));
        float3 env = texCUBE(u_EnvironmentMap, R).rgb;
        // Single control: 0 = no reflection, 1 = full mirror
        result = lerp(result, env, u_ReflectionStrength);
    }
    
    // Output final color (alpha drives Alpha / Additive blend modes)
    return float4(result, alpha);
}
