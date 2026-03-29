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

struct Light
{
    uint type;
    uint flags;
    uint dataIndex;
    uint shadowIndex;
    vec4 colorIntensity;
    vec4 specular;
};

struct DirectionalLight
{
    vec4 direction;
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowBiasParams;
    vec4 shadowState;
    mat4 shadowMatrices[3];
    mat4 shadowInvMatrices[3];
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
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

layout(binding = 10, std430) readonly buffer LightsSSBO
{
    uvec4 counts;
    Light lights[64];
    DirectionalLight directionalLights[64];
} lightData;

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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + ((vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0));
}

uint GetSpecularHighlightType(vec4 lightSpecular)
{
    uint t = uint(lightSpecular.w + 0.5);
    if ((t < 1u) || (t > 3u))
    {
        t = 3u;
    }
    return t;
}

vec3 SpecularPhong(vec3 N, vec3 V, vec3 L, vec3 F, float shininess)
{
    vec3 R = reflect(-L, N);
    float s = pow(max(dot(R, V), 0.0), shininess);
    return F * s;
}

vec3 SpecularBlinnPhong(vec3 N, vec3 H, vec3 F, float shininess)
{
    float s = pow(max(dot(N, H), 0.0), shininess);
    return F * s;
}

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

vec3 SpecularCookTorrance(vec3 N, vec3 V, vec3 L, vec3 H, vec3 F, float effectiveRoughness)
{
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 param = N;
    vec3 param_1 = H;
    float param_2 = effectiveRoughness;
    float D = DistributionGGX(param, param_1, param_2);
    vec3 param_3 = N;
    vec3 param_4 = V;
    vec3 param_5 = L;
    float param_6 = effectiveRoughness;
    float G = GeometrySmith(param_3, param_4, param_5, param_6);
    vec3 numerator = F * (D * G);
    float denominator = max((4.0 * NdotV) * NdotL, 9.9999997473787516355514526367188e-06);
    return numerator / vec3(denominator);
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
    mat3 normalMat = transpose(inverse(mat3(inst.model[0].xyz, inst.model[1].xyz, inst.model[2].xyz)));
    vec3 N = normalize(normalMat * inNormal);
    vec3 V = normalize(camera.cameraPosition.xyz - worldPos.xyz);
    vec3 directLight = vec3(0.0);
    uint lightCount = min(lightData.counts.x, 64u);
    Light light;
    DirectionalLight d;
    for (uint i = 0u; i < lightCount; i++)
    {
        light.type = lightData.lights[i].type;
        light.flags = lightData.lights[i].flags;
        light.dataIndex = lightData.lights[i].dataIndex;
        light.shadowIndex = lightData.lights[i].shadowIndex;
        light.colorIntensity = lightData.lights[i].colorIntensity;
        light.specular = lightData.lights[i].specular;
        if ((light.flags & 1u) == 0u)
        {
            continue;
        }
        if (light.type == 0u)
        {
            d.direction = lightData.directionalLights[light.dataIndex].direction;
            d.shadowCascadeSplits = lightData.directionalLights[light.dataIndex].shadowCascadeSplits;
            d.shadowParams = lightData.directionalLights[light.dataIndex].shadowParams;
            d.shadowBiasParams = lightData.directionalLights[light.dataIndex].shadowBiasParams;
            d.shadowState = lightData.directionalLights[light.dataIndex].shadowState;
            d.shadowMatrices[0] = lightData.directionalLights[light.dataIndex].shadowMatrices[0];
            d.shadowMatrices[1] = lightData.directionalLights[light.dataIndex].shadowMatrices[1];
            d.shadowMatrices[2] = lightData.directionalLights[light.dataIndex].shadowMatrices[2];
            d.shadowInvMatrices[0] = lightData.directionalLights[light.dataIndex].shadowInvMatrices[0];
            d.shadowInvMatrices[1] = lightData.directionalLights[light.dataIndex].shadowInvMatrices[1];
            d.shadowInvMatrices[2] = lightData.directionalLights[light.dataIndex].shadowInvMatrices[2];
            vec3 L = normalize(-d.direction.xyz);
            vec3 H = normalize(V + L);
            vec3 lightColor = light.colorIntensity.xyz * light.colorIntensity.w;
            float specularStrength = max(light.specular.x, 0.0);
            float shininess = max(light.specular.y, 1.0);
            float NdotL = max(dot(N, L), 0.0);
            vec3 F0 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
            float fresnelAmount = clamp(light.specular.z, 0.0, 1.0);
            float param = max(dot(H, V), 0.0);
            vec3 param_1 = F0;
            vec3 Fschlick = FresnelSchlick(param, param_1);
            vec3 F = mix(F0, Fschlick, vec3(fresnelAmount));
            float smoothness = 1.0 - roughness;
            vec4 param_2 = light.specular;
            uint highlightType = GetSpecularHighlightType(param_2);
            vec3 specularBrdf = vec3(0.0);
            if (highlightType == 1u)
            {
                vec3 param_3 = N;
                vec3 param_4 = V;
                vec3 param_5 = L;
                vec3 param_6 = F;
                float param_7 = shininess;
                specularBrdf = SpecularPhong(param_3, param_4, param_5, param_6, param_7);
            }
            else
            {
                if (highlightType == 2u)
                {
                    vec3 param_8 = N;
                    vec3 param_9 = H;
                    vec3 param_10 = F;
                    float param_11 = shininess;
                    specularBrdf = SpecularBlinnPhong(param_8, param_9, param_10, param_11);
                }
                else
                {
                    float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.0500000007450580596923828125, 1.0);
                    float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.04500000178813934326171875);
                    vec3 param_12 = N;
                    vec3 param_13 = V;
                    vec3 param_14 = L;
                    vec3 param_15 = H;
                    vec3 param_16 = F;
                    float param_17 = effectiveRoughness;
                    specularBrdf = SpecularCookTorrance(param_12, param_13, param_14, param_15, param_16, param_17);
                }
            }
            vec3 specular = (specularBrdf * specularStrength) * (smoothness * smoothness);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
            vec3 diffuse = (kD * albedo) / vec3(3.1415927410125732421875);
            directLight += (((diffuse + specular) * lightColor) * NdotL);
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

