#version 450

// TEMPORARY: this Mesh pipeline is a bring-up shader.
// It will be replaced by Main.* once full scene/material plumbing is wired.

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
layout(set = 0, binding = 2) uniform sampler2D surfaceAtlas;
layout(set = 0, binding = 6) uniform sampler2D neonTexture;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
} pushData;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragLocalPos;
layout(location = 3) in vec3 fragLocalNormal;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outGlowColor;

const float PI = 3.14159265359;
const float EMISSIVE_SCENE_BOOST = 4.0;
const float EMISSIVE_GLOW_BOOST = 8.0;
const float BLACK_NEON_GLOW_FLOOR = 1.0 / 255.0;
const float BLACK_NEON_GLOW_EXTRA_BOOST = 2.0;
const int SMOOTH = 0;

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

int GetTopSurfaceType() { return int(pushData.surfaceData0.x + 0.5); }
int GetBottomSurfaceType() { return int(pushData.surfaceData0.y + 0.5); }
int GetFrontSurfaceType() { return int(pushData.surfaceData0.z + 0.5); }
int GetBackSurfaceType() { return int(pushData.surfaceData0.w + 0.5); }
int GetLeftSurfaceType() { return int(pushData.surfaceData1.x + 0.5); }
int GetRightSurfaceType() { return int(pushData.surfaceData1.y + 0.5); }
bool IsSurfaceEnabled() { return pushData.surfaceData1.z > 0.5; }

vec3 GetMeshSizeFromModel() {
    vec3 xAxis = vec3(pushData.model[0][0], pushData.model[0][1], pushData.model[0][2]);
    vec3 yAxis = vec3(pushData.model[1][0], pushData.model[1][1], pushData.model[1][2]);
    vec3 zAxis = vec3(pushData.model[2][0], pushData.model[2][1], pushData.model[2][2]);
    return vec3(length(xAxis), length(yAxis), length(zAxis));
}

int GetSurfaceType(vec3 normal) {
    vec3 n = normalize(normal);
    vec3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) {
        return (n.y > 0.0) ? GetTopSurfaceType() : GetBottomSurfaceType();
    }
    if (a.x >= a.y && a.x >= a.z) {
        return (n.x > 0.0) ? GetRightSurfaceType() : GetLeftSurfaceType();
    }
    return (n.z > 0.0) ? GetFrontSurfaceType() : GetBackSurfaceType();
}

vec2 GetFaceUV(vec3 localPos, vec3 localNormal) {
    vec3 scaledPos = localPos * GetMeshSizeFromModel();
    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    vec2 uv;

    if (a.x >= a.y && a.x >= a.z) {
        uv = scaledPos.zy;
    } else if (a.y >= a.x && a.y >= a.z) {
        uv = scaledPos.xz;
    } else {
        uv = scaledPos.xy;
    }
    return fract(uv * vec2(0.5, 0.5));
}

vec2 GetSurfaceAtlasGrid() {
    vec2 atlasSize = vec2(textureSize(surfaceAtlas, 0));
    float cols = max(1.0, round(atlasSize.x / max(atlasSize.y, 1.0)));
    return vec2(cols, 1.0);
}

vec2 GetSurfaceAtlasUV(int surfaceType, vec2 uv) {
    vec2 grid = GetSurfaceAtlasGrid();
    float cols = max(1.0, grid.x);
    float rows = max(1.0, grid.y);
    float tileWidth = 1.0 / cols;
    float tileHeight = 1.0 / rows;
    float tileIndex = clamp(float(surfaceType - 1), 0.0, (cols * rows) - 1.0);
    float tileX = mod(tileIndex, cols);
    float tileY = floor(tileIndex / cols);
    vec2 base = vec2(tileX * tileWidth, tileY * tileHeight);
    vec2 tileUV = uv * vec2(tileWidth, tileHeight);
    return base + tileUV;
}

vec3 SampleSurfaceColor(int surfaceType, vec2 uv) {
    if (surfaceType == SMOOTH || !IsSurfaceEnabled()) {
        return vec3(1.0);
    }
    return texture(surfaceAtlas, GetSurfaceAtlasUV(surfaceType, uv)).rgb;
}

void main() {
    vec3 albedo = pushData.baseColor.rgb;
    int surfaceType = GetSurfaceType(fragLocalNormal);
    vec2 faceUV = GetFaceUV(fragLocalPos, fragLocalNormal);
    vec3 surfaceDetail = SampleSurfaceColor(surfaceType, faceUV);
    albedo *= surfaceDetail;
    float alpha = pushData.baseColor.a;
    float ignoreLighting = pushData.material.w;
    float metalness = clamp(pushData.material.x, 0.0, 1.0);
    float roughness = clamp(pushData.material.y, 0.0, 1.0);
    float emissive = max(pushData.material.z, 0.0);
    float neonEnabled = camera.renderSettings.z;
    float allowBlackNeon = camera.renderSettings.w;
    vec3 emissiveScene = albedo * emissive * EMISSIVE_SCENE_BOOST;

    float albedoL2 = dot(albedo, albedo);
    float glowMask = (neonEnabled > 0.5 && ignoreLighting <= 0.5 && emissive > 1e-4) ? 1.0 : 0.0;
    vec3 glowBase = albedo;
    float blackNeonGlowBoost = 1.0;
    if (allowBlackNeon > 0.5 && albedoL2 < 1e-6 && glowMask > 0.0) {
        glowBase = vec3(BLACK_NEON_GLOW_FLOOR);
        blackNeonGlowBoost = BLACK_NEON_GLOW_EXTRA_BOOST;
    }
    vec3 glowColor = glowBase * emissive * EMISSIVE_GLOW_BOOST * blackNeonGlowBoost * glowMask;
    vec2 neonUv = gl_FragCoord.xy / vec2(max(textureSize(neonTexture, 0), ivec2(1)));
    vec3 neonSample = (neonEnabled > 0.5) ? texture(neonTexture, neonUv).rgb : vec3(0.0);

    if (ignoreLighting > 0.5) {
        outSceneColor = vec4(albedo + emissiveScene + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(0.0);
        return;
    }

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);
    vec3 color = camera.ambient.rgb * albedo;

    vec3 L = normalize(-camera.lightDirection.xyz);
    vec3 H = normalize(V + L);

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
    color += (diffuse + specular) * lightColor * NdotL;

    color += emissiveScene;
    outSceneColor = vec4(color + ((neonSample * 0.1) * glowMask), alpha);
    outGlowColor = vec4(glowColor, glowMask);
}
