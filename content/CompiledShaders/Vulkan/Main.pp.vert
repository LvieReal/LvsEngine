#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings; // x: perVertexShadingEnabled
    vec4 cameraForward;
} camera;


// --- include begin: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\LightingTypes.glsl

#ifndef LVS_UTIL_LIGHTING_TYPES_GLSL
#define LVS_UTIL_LIGHTING_TYPES_GLSL

struct Light {
    uint type;
    uint flags;
    uint dataIndex;
    uint shadowIndex;
    vec4 colorIntensity;
    vec4 specular; // x: strength, y: shininess, z: fresnelAmount, w: highlightType
};

struct DirectionalLight {
    vec4 direction;
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowBiasParams;
    vec4 shadowState;
    mat4 shadowMatrices[3];
    mat4 shadowInvMatrices[3];
};

layout(set = 0, binding = 10, std430) readonly buffer LightsSSBO {
    uvec4 counts; // x: lightCount
    Light lights[64];
    DirectionalLight directionalLights[64];
} lightData;

#endif



// --- include end: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\LightingTypes.glsl

struct InstanceData {
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

layout(set = 0, binding = 9, std430) readonly buffer InstanceSSBO {
    InstanceData instances[];
} instanceData;

layout(push_constant) uniform PushConstants {
    uvec4 data; // data.x: base instance
} pushData;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec4 fragBaseColor;
layout(location = 3) out vec4 fragMaterial;
layout(location = 4) out vec3 fragVertexLighting;
layout(location = 5) out vec3 fragLocalPos;
layout(location = 6) out vec3 fragLocalNormal;
layout(location = 7) flat out uint fragInstanceIndex;


// --- include begin: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\LightingBRDF.glsl

#ifndef LVS_UTIL_LIGHTING_BRDF_GLSL
#define LVS_UTIL_LIGHTING_BRDF_GLSL

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 1e-5);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 1e-5);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

uint GetSpecularHighlightType(vec4 lightSpecular) {
    uint t = uint(lightSpecular.w + 0.5);
    if (t < 1u || t > 3u) {
        t = 3u;
    }
    return t;
}

vec3 SpecularPhong(vec3 N, vec3 V, vec3 L, vec3 F, float shininess) {
    vec3 R = reflect(-L, N);
    float s = pow(max(dot(R, V), 0.0), shininess);
    return F * s;
}

vec3 SpecularBlinnPhong(vec3 N, vec3 H, vec3 F, float shininess) {
    float s = pow(max(dot(N, H), 0.0), shininess);
    return F * s;
}

vec3 SpecularCookTorrance(vec3 N, vec3 V, vec3 L, vec3 H, vec3 F, float effectiveRoughness) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    float D = DistributionGGX(N, H, effectiveRoughness);
    float G = GeometrySmith(N, V, L, effectiveRoughness);
    vec3 numerator = D * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 1e-5);
    return numerator / denominator;
}

#endif



// --- include end: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\LightingBRDF.glsl

void main() {
    uint instanceIndex = pushData.data.x + gl_InstanceIndex;
    InstanceData inst = instanceData.instances[instanceIndex];

    vec4 worldPos = inst.model * vec4(inPosition, 1.0);
    gl_Position = camera.projection * camera.view * worldPos;

    vec3 albedo = inst.baseColor.rgb;
    float metalness = clamp(inst.material.x, 0.0, 1.0);
    float roughness = clamp(inst.material.y, 0.0, 1.0);
    float emissive = max(inst.material.z, 0.0);

    mat3 normalMat = transpose(inverse(mat3(inst.model)));
    vec3 N = normalize(normalMat * inNormal);
    vec3 V = normalize(camera.cameraPosition.xyz - worldPos.xyz);
    vec3 directLight = vec3(0.0);

    uint lightCount = min(lightData.counts.x, uint(64));
    for (uint i = 0; i < lightCount; ++i) {
        Light light = lightData.lights[i];
        if ((light.flags & 1u) == 0u) {
            continue;
        }
        if (light.type == 0u) {
            DirectionalLight d = lightData.directionalLights[light.dataIndex];
            vec3 L = normalize(-d.direction.xyz);
            vec3 H = normalize(V + L);

            vec3 lightColor = light.colorIntensity.rgb * light.colorIntensity.a;
            float specularStrength = max(light.specular.x, 0.0);
            float shininess = max(light.specular.y, 1.0);

            float NdotL = max(dot(N, L), 0.0);
            vec3 F0 = mix(vec3(0.04), albedo, metalness);

            float fresnelAmount = clamp(light.specular.z, 0.0, 1.0);
            vec3 Fschlick = FresnelSchlick(max(dot(H, V), 0.0), F0);
            vec3 F = mix(F0, Fschlick, fresnelAmount);

            float smoothness = 1.0 - roughness;
            uint highlightType = GetSpecularHighlightType(light.specular);
            vec3 specularBrdf = vec3(0.0);
            if (highlightType == 1u) {
                specularBrdf = SpecularPhong(N, V, L, F, shininess);
            } else if (highlightType == 2u) {
                specularBrdf = SpecularBlinnPhong(N, H, F, shininess);
            } else {
                float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.05, 1.0);
                float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.045);
                specularBrdf = SpecularCookTorrance(N, V, L, H, F, effectiveRoughness);
            }
            vec3 specular = specularBrdf * specularStrength * (smoothness * smoothness);

            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
            vec3 diffuse = kD * albedo / PI;
            directLight += (diffuse + specular) * lightColor * NdotL;
        }
    }

    fragNormal = N;
    fragWorldPos = worldPos.xyz;
    fragBaseColor = inst.baseColor;
    fragMaterial = inst.material;
    fragVertexLighting = directLight;
    fragLocalPos = inPosition;
    fragLocalNormal = inNormal;
    fragInstanceIndex = instanceIndex;
}

