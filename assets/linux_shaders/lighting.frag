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
uniform vec3 u_DiffuseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_ReflectionStrength;
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
        // Sample normal from texture and convert from [0,1] to [-1,1]
        vec3 normalMap = normalize(texture2D(u_NormalTexture, vTexCoord).rgb * 2.0 - 1.0);
        
        // Transform from tangent space to world space
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
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
    
    // Get diffuse color
    vec3 diffuseColor = u_DiffuseColor;
    if (u_HasDiffuseTexture) {
        diffuseColor = texture2D(u_DiffuseTexture, vTexCoord).rgb;
    }
    
    // Initialize lighting result
    vec3 result = vec3(0.0);
    
    // Calculate lighting from all lights
    for (int i = 0; i < u_NumLights; i++) {
        Light light = u_Lights[i];
        float lightType = light.position.w;
        
        if (lightType == 0.0) {
            // Directional light
            result += calculateDirectionalLight(light, normal, viewDir);
        } else if (lightType == 1.0) {
            // Point light
            result += calculatePointLight(light, normal, viewDir, vWorldPos);
        } else if (lightType == 2.0) {
            // Spot light
            result += calculateSpotLight(light, normal, viewDir, vWorldPos);
        }
    }
    
    // Apply material properties
    vec3 ambient = vec3(0.1) * diffuseColor * ao; // Ambient lighting with AO
    result = ambient + result * diffuseColor;

    if (u_HasEnvironmentMap && u_ReflectionStrength > 0.0) {
        vec3 I = normalize(vWorldPos - u_CameraPos);
        vec3 R = reflect(I, normalize(normal));
        vec3 env = textureCube(u_EnvironmentMap, R).rgb;
        // Single control: 0 = no reflection, 1 = full mirror
        result = mix(result, env, u_ReflectionStrength);
    }
    
    // Output final color
    gl_FragColor = vec4(result, 1.0);
}
