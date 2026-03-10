#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform CameraUBO {
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
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowState;
    vec4 cameraForward;
} camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
} pushData;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec4 fragBaseColor;
layout(location = 3) out vec4 fragMaterial;
layout(location = 4) out vec3 fragVertexLighting;
layout(location = 5) out vec3 fragLocalPos;
layout(location = 6) out vec3 fragLocalNormal;

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

void main() {
    vec4 worldPos = pushData.model * vec4(inPosition, 1.0);
    gl_Position = camera.projection * camera.view * worldPos;

    vec3 albedo = pushData.baseColor.rgb;
    float metalness = clamp(pushData.material.x, 0.0, 1.0);
    float roughness = clamp(pushData.material.y, 0.0, 1.0);
    float emissive = max(pushData.material.z, 0.0);

    vec3 N = normalize(mat3(pushData.model) * inNormal);
    vec3 V = normalize(camera.cameraPosition.xyz - worldPos.xyz);
    vec3 L = normalize(-camera.lightDirection.xyz);
    vec3 H = normalize(V + L);

    vec3 directLight = vec3(0.0);

    if (camera.renderSettings.x > 0.5) {
        vec3 lightColor = camera.lightColorIntensity.rgb * camera.lightColorIntensity.a;
        float specularStrength = max(camera.lightSpecular.x, 0.0);
        float shininess = max(camera.lightSpecular.y, 1.0);

        float NdotL = max(dot(N, L), 0.0);
        vec3 F0 = mix(vec3(0.04), albedo, metalness);

        float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.05, 1.0);
        float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.045);
        float NDF = DistributionGGX(N, H, effectiveRoughness);
        float G = GeometrySmith(N, V, L, effectiveRoughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = max(4.0 * max(dot(N, V), 0.0) * NdotL, 1e-5);
        float smoothness = 1.0 - roughness;
        vec3 specular = (numerator / denominator) * specularStrength * (smoothness * smoothness);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
        vec3 diffuse = kD * albedo / PI;
        directLight += (diffuse + specular) * lightColor * NdotL;
    }

    fragNormal = N;
    fragWorldPos = worldPos.xyz;
    fragBaseColor = pushData.baseColor;
    fragMaterial = pushData.material;
    fragVertexLighting = directLight;
    fragLocalPos = inPosition;
    fragLocalNormal = inNormal;
}
