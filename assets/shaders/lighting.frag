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
uniform float3 u_DiffuseColor;
uniform float u_Metallic;
uniform float u_Roughness;

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
        // Sample normal from texture and convert from [0,1] to [-1,1]
        float3 normalMap = normalize(tex2D(u_NormalTexture, input.texCoord).rgb * 2.0f - 1.0f);
        
        // Transform from tangent space to world space
        float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
        return normalize(mul(normalMap, TBN));
    } else {
        return normalize(input.normal);
    }
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
    
    // Sample diffuse texture if available
    if (u_HasDiffuseTexture) {
        float4 diffuseSample = tex2D(u_DiffuseTexture, input.texCoord);
        diffuseColor = diffuseSample.rgb;
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
    
    // Calculate lighting from all lights
    for (int i = 0; i < u_NumLights; i++) {
        Light light = u_Lights[i];
        float lightType = light.position.w;
        
        if (lightType == 0.0f) {
            // Directional light
            result += calculateDirectionalLight(light, normal, viewDir);
        } else if (lightType == 1.0f) {
            // Point light
            result += calculatePointLight(light, normal, viewDir, input.worldPos);
        } else if (lightType == 2.0f) {
            // Spot light
            result += calculateSpotLight(light, normal, viewDir, input.worldPos);
        }
    }
    
    // Apply material properties with textures
    float3 ambient = float3(0.1f, 0.1f, 0.1f) * diffuseColor * ambientOcclusion; // Ambient lighting with AO
    result = ambient + result * diffuseColor;
    
    // Output final color
    return float4(result, 1.0f);
}
