#version 450
#ifdef GL_ARB_shader_draw_parameters
#extension GL_ARB_shader_draw_parameters : enable
#endif

struct InstanceData
{
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

layout(binding = 9, std430) readonly buffer InstanceSSBO
{
    InstanceData instances[];
} instanceData;

layout(binding = 0, std140) uniform CameraUBO
{
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 lightSpecular;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    mat4 shadowMatrices[3];
    mat4 shadowInvMatrices[3];
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowAdaptiveBiasParams;
    vec4 shadowState;
    vec4 cameraForward;
} camera;

struct PushConstants
{
    uvec4 data;
};

uniform PushConstants pushData;

#ifdef GL_ARB_shader_draw_parameters
#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB
#else
uniform int SPIRV_Cross_BaseInstance;
#endif
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec4 fragBaseColor;
layout(location = 3) out vec4 fragMaterial;
layout(location = 4) out vec3 fragVertexLighting;
layout(location = 5) out vec3 fragLocalPos;
layout(location = 6) out vec3 fragLocalNormal;
layout(location = 7) flat out uint fragInstanceIndex;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return num / max(denom, 9.9999997473787516355514526367188e-06);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = (NdotV * (1.0 - k)) + k;
    return num / max(denom, 9.9999997473787516355514526367188e-06);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float param = NdotV;
    float param_1 = roughness;
    float ggx2 = GeometrySchlickGGX(param, param_1);
    float param_2 = NdotL;
    float param_3 = roughness;
    float ggx1 = GeometrySchlickGGX(param_2, param_3);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + ((vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0));
}

void main()
{
    uint instanceIndex = pushData.data.x + uint((gl_InstanceID + SPIRV_Cross_BaseInstance));
    InstanceData inst;
    inst.model = instanceData.instances[instanceIndex].model;
    inst.baseColor = instanceData.instances[instanceIndex].baseColor;
    inst.material = instanceData.instances[instanceIndex].material;
    inst.surfaceData0 = instanceData.instances[instanceIndex].surfaceData0;
    inst.surfaceData1 = instanceData.instances[instanceIndex].surfaceData1;
    vec4 worldPos = inst.model * vec4(inPosition, 1.0);
    gl_Position = (camera.projection * camera.view) * worldPos;
    vec3 albedo = inst.baseColor.xyz;
    float metalness = clamp(inst.material.x, 0.0, 1.0);
    float roughness = clamp(inst.material.y, 0.0, 1.0);
    float emissive = max(inst.material.z, 0.0);
    vec3 N = normalize(mat3(inst.model[0].xyz, inst.model[1].xyz, inst.model[2].xyz) * inNormal);
    vec3 V = normalize(camera.cameraPosition.xyz - worldPos.xyz);
    vec3 L = normalize(-camera.lightDirection.xyz);
    vec3 H = normalize(V + L);
    vec3 directLight = vec3(0.0);
    vec3 lightColor = camera.lightColorIntensity.xyz * camera.lightColorIntensity.w;
    float specularStrength = max(camera.lightSpecular.x, 0.0);
    float shininess = max(camera.lightSpecular.y, 1.0);
    float NdotL = max(dot(N, L), 0.0);
    vec3 F0 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
    float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.0500000007450580596923828125, 1.0);
    float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.04500000178813934326171875);
    vec3 param = N;
    vec3 param_1 = H;
    float param_2 = effectiveRoughness;
    float NDF = DistributionGGX(param, param_1, param_2);
    vec3 param_3 = N;
    vec3 param_4 = V;
    vec3 param_5 = L;
    float param_6 = effectiveRoughness;
    float G = GeometrySmith(param_3, param_4, param_5, param_6);
    float fresnelAmount = clamp(camera.lightSpecular.z, 0.0, 1.0);
    float param_7 = max(dot(H, V), 0.0);
    vec3 param_8 = F0;
    vec3 Fschlick = FresnelSchlick(param_7, param_8);
    vec3 F = mix(F0, Fschlick, vec3(fresnelAmount));
    vec3 numerator = F * (NDF * G);
    float denominator = max((4.0 * max(dot(N, V), 0.0)) * NdotL, 9.9999997473787516355514526367188e-06);
    float smoothness = 1.0 - roughness;
    vec3 specular = ((numerator / vec3(denominator)) * specularStrength) * (smoothness * smoothness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
    vec3 diffuse = (kD * albedo) / vec3(3.1415927410125732421875);
    directLight += (((diffuse + specular) * lightColor) * NdotL);
    fragNormal = N;
    fragWorldPos = worldPos.xyz;
    fragBaseColor = inst.baseColor;
    fragMaterial = inst.material;
    fragVertexLighting = directLight;
    fragLocalPos = inPosition;
    fragLocalNormal = inNormal;
    fragInstanceIndex = instanceIndex;
}

