#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec4 fragBaseColor;
layout(location = 3) in vec4 fragMaterial;
layout(location = 4) in vec3 fragVertexLighting;
layout(location = 5) in vec3 fragLocalPos;
layout(location = 6) in vec3 fragLocalNormal;
layout(location = 7) flat in uint fragInstanceIndex;

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

layout(set = 0, binding = 1) uniform samplerCube skyboxTex;
layout(set = 0, binding = 2) uniform sampler2D surfaceAtlas;
layout(set = 0, binding = 3) uniform sampler2DShadow directionalShadowMap0;
layout(set = 0, binding = 4) uniform sampler2DShadow directionalShadowMap1;
layout(set = 0, binding = 5) uniform sampler2DShadow directionalShadowMap2;
layout(set = 0, binding = 6) uniform sampler2D neonTexture;
layout(set = 0, binding = 7) uniform sampler3D directionalShadowJitter;
layout(set = 0, binding = 8) uniform sampler2D surfaceNormalAtlas;

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

InstanceData inst;

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

int GetTopSurfaceType() { return int(inst.surfaceData0.x + 0.5); }
int GetBottomSurfaceType() { return int(inst.surfaceData0.y + 0.5); }
int GetFrontSurfaceType() { return int(inst.surfaceData0.z + 0.5); }
int GetBackSurfaceType() { return int(inst.surfaceData0.w + 0.5); }
int GetLeftSurfaceType() { return int(inst.surfaceData1.x + 0.5); }
int GetRightSurfaceType() { return int(inst.surfaceData1.y + 0.5); }
bool IsSurfaceEnabled() { return inst.surfaceData1.z > 0.5; }
bool IsSurfaceNormalEnabled() { return inst.surfaceData1.w > 0.5; }

vec3 GetMeshSizeFromModel() {
    vec3 xAxis = vec3(inst.model[0][0], inst.model[0][1], inst.model[0][2]);
    vec3 yAxis = vec3(inst.model[1][0], inst.model[1][1], inst.model[1][2]);
    vec3 zAxis = vec3(inst.model[2][0], inst.model[2][1], inst.model[2][2]);
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
    vec3 size = GetMeshSizeFromModel();
    vec3 scaledPos = localPos * size;
    vec3 halfSize = size * 0.5;
    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    vec2 uv;

    if (a.x >= a.y && a.x >= a.z) {
        uv = scaledPos.zy + halfSize.zy;
    } else if (a.y >= a.x && a.y >= a.z) {
        uv = scaledPos.xz + halfSize.xz;
    } else {
        uv = scaledPos.xy + halfSize.xy;
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

vec3 GetSurfaceMappedNormal(vec3 localNormal, int surfaceType, vec2 uv) {
    if (!IsSurfaceEnabled() || !IsSurfaceNormalEnabled() || surfaceType == SMOOTH) {
        return normalize(mat3(inst.model) * localNormal);
    }

    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    vec3 tangentLocal;
    vec3 bitangentLocal;
    if (a.x >= a.y && a.x >= a.z) {
        tangentLocal = vec3(0.0, 0.0, 1.0);
        bitangentLocal = vec3(0.0, 1.0, 0.0);
    } else if (a.y >= a.x && a.y >= a.z) {
        tangentLocal = vec3(1.0, 0.0, 0.0);
        bitangentLocal = vec3(0.0, 0.0, 1.0);
    } else {
        tangentLocal = vec3(1.0, 0.0, 0.0);
        bitangentLocal = vec3(0.0, 1.0, 0.0);
    }

    vec3 sampled = texture(surfaceNormalAtlas, GetSurfaceAtlasUV(surfaceType, uv)).rgb;
    vec3 tangentSpaceNormal = normalize((sampled * 2.0) - 1.0);
    vec3 mappedLocal = normalize(
        tangentLocal * tangentSpaceNormal.x +
        bitangentLocal * tangentSpaceNormal.y +
        n * tangentSpaceNormal.z
    );
    return normalize(mat3(inst.model) * mappedLocal);
}

const int SHADOW_SAMPLES_COUNT = 64;
const int SHADOW_SAMPLES_HALF = SHADOW_SAMPLES_COUNT / 2;
const float INV_SHADOW_SAMPLES = 1.0 / float(SHADOW_SAMPLES_COUNT);

vec2 GetDirectionalShadowTexelSize(int cascadeIndex) {
    ivec2 size = ivec2(1, 1);
    if (cascadeIndex == 1) {
        size = max(textureSize(directionalShadowMap1, 0), ivec2(1));
    } else if (cascadeIndex == 2) {
        size = max(textureSize(directionalShadowMap2, 0), ivec2(1));
    } else {
        size = max(textureSize(directionalShadowMap0, 0), ivec2(1));
    }
    return 1.0 / vec2(size);
}

float SampleDirectionalShadowMap(int cascadeIndex, vec3 shadowCoord) {
    if (cascadeIndex == 1) {
        return texture(directionalShadowMap1, shadowCoord);
    }
    if (cascadeIndex == 2) {
        return texture(directionalShadowMap2, shadowCoord);
    }
    return texture(directionalShadowMap0, shadowCoord);
}

float SampleDirectionalAdaptivePCF64(int cascadeIndex, vec3 baseShadowCoord, float radiusTexels, float ndotl) {
    float radius = max(0.0, radiusTexels);
    if (radius <= 1e-6) {
        return SampleDirectionalShadowMap(cascadeIndex, baseShadowCoord);
    }

    vec2 texelSize = GetDirectionalShadowTexelSize(cascadeIndex);
    float texelScale = max(texelSize.x, texelSize.y);
    float fsize = radius * texelScale;
    vec3 jcoord = vec3(gl_FragCoord.xy * camera.shadowState.yz, 0.0);
    float shadow = 0.0;
    vec3 smCoord = baseShadowCoord;

    // Cheap pretest: 8 taps via 4 jitter fetches.
    for (int i = 0; i < 4; ++i) {
        vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
        jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

        smCoord.xy = (offset.xy * fsize) + baseShadowCoord.xy;
        shadow += SampleDirectionalShadowMap(cascadeIndex, smCoord) * (1.0 / 8.0);

        smCoord.xy = (offset.zw * fsize) + baseShadowCoord.xy;
        shadow += SampleDirectionalShadowMap(cascadeIndex, smCoord) * (1.0 / 8.0);
    }

    if (ndotl > 0.0 && shadow > 0.0 && shadow < 1.0) {
        shadow *= (1.0 / 8.0);
        for (int i = 0; i < (SHADOW_SAMPLES_HALF - 4); ++i) {
            vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
            jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

            smCoord.xy = (offset.xy * fsize) + baseShadowCoord.xy;
            shadow += SampleDirectionalShadowMap(cascadeIndex, smCoord) * INV_SHADOW_SAMPLES;

            smCoord.xy = (offset.zw * fsize) + baseShadowCoord.xy;
            shadow += SampleDirectionalShadowMap(cascadeIndex, smCoord) * INV_SHADOW_SAMPLES;
        }
    }

    return shadow;
}

float ComputeDirectionalBias(int cascadeIndex, float pcfRadiusTexels, vec3 normal, vec3 lightDir) {
    vec2 texelSize = GetDirectionalShadowTexelSize(cascadeIndex);
    float texelScale = max(texelSize.x, texelSize.y);
    float cascadeResolution = 1.0 / max(texelScale, 1e-6);
    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    float slope = sqrt(max(1.0 - ndotl * ndotl, 0.0)) / max(ndotl, 0.05);
    float biasTexels = camera.shadowParams.x + (0.25 * slope);
    biasTexels *= (1.0 + pcfRadiusTexels * 0.25);
    const float biasReferenceResolution = 2048.0;
    float lowResRatio = clamp(biasReferenceResolution / max(cascadeResolution, 1.0), 1.0, 8.0);
    float lowResCompensation = sqrt(lowResRatio);
    return (biasTexels * texelScale) / lowResCompensation;
}

float ComputeShadowFactor(vec3 normal, vec3 lightDir) {
    if (camera.shadowState.x < 0.5) {
        return 1.0;
    }

    int cascadeCount = clamp(int(camera.shadowCascadeSplits.w + 0.5), 1, 3);
    float viewDepth = max(dot(fragWorldPos - camera.cameraPosition.xyz, normalize(camera.cameraForward.xyz)), 0.0);
    int cascadeIndex = 0;
    if (cascadeCount >= 3 && viewDepth > camera.shadowCascadeSplits.y) {
        cascadeIndex = 2;
    } else if (cascadeCount >= 2 && viewDepth > camera.shadowCascadeSplits.x) {
        cascadeIndex = 1;
    }

    vec4 lightClip = camera.shadowMatrices[cascadeIndex] * vec4(fragWorldPos, 1.0);
    if (abs(lightClip.w) <= 1e-6) {
        return 1.0;
    }

    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec2 shadowUv = (lightNdc.xy * 0.5) + 0.5;
    float receiverDepth = lightNdc.z;
    if (receiverDepth <= 0.0 || receiverDepth >= 1.0) {
        return 1.0;
    }
    if (shadowUv.x < 0.0 || shadowUv.x > 1.0 || shadowUv.y < 0.0 || shadowUv.y > 1.0) {
        return 1.0;
    }

    float softness = camera.shadowParams.y;
    if (cascadeIndex == 1) {
        softness *= 0.65;
    } else if (cascadeIndex == 2) {
        softness *= 0.45;
    }
    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    float bias = ComputeDirectionalBias(cascadeIndex, softness, normal, lightDir);
    float compareDepth = clamp(receiverDepth - bias, 0.0, 1.0);
    float shadow = SampleDirectionalAdaptivePCF64(cascadeIndex, vec3(shadowUv, compareDepth), softness, ndotl);

    if (cascadeIndex == cascadeCount - 1) {
        float fadeWidth = clamp(camera.shadowParams.w, 0.0, 1.0);
        if (fadeWidth > 1e-4) {
            float fadeStart = camera.shadowCascadeSplits.z * (1.0 - fadeWidth);
            float fade = clamp((viewDepth - fadeStart) / max(camera.shadowCascadeSplits.z - fadeStart, 1e-4), 0.0, 1.0);
            shadow = mix(shadow, 1.0, fade);
        }
    }
    return shadow;
}

void main() {
    inst = instanceData.instances[fragInstanceIndex];

    vec3 albedo = fragBaseColor.rgb;
    float alpha = fragBaseColor.a;
    int surfaceType = GetSurfaceType(fragLocalNormal);
    vec2 faceUV = GetFaceUV(fragLocalPos, fragLocalNormal);
    vec3 surfaceDetail = SampleSurfaceColor(surfaceType, faceUV);
    albedo *= surfaceDetail;

    float ignoreLighting = fragMaterial.w;

    float metalness = clamp(fragMaterial.x, 0.0, 1.0);
    float roughness = clamp(fragMaterial.y, 0.0, 1.0);
    float emissive = max(fragMaterial.z, 0.0);
    bool neonEnabled = camera.renderSettings.z > 0.5;
    bool allowBlackNeon = (camera.renderSettings.w > 0.5) && neonEnabled;

    float albedoL2 = dot(albedo, albedo);
    float glowMask = (neonEnabled && ignoreLighting <= 0.5 && emissive > 1e-4) ? 1.0 : 0.0;
    if (!allowBlackNeon && albedoL2 < 1e-6) {
        glowMask = 0.0;
    }
    vec3 glowBase = albedo;
    float blackNeonGlowBoost = 1.0;
    if (allowBlackNeon && albedoL2 < 1e-6) {
        // Keep black-neon visible without turning the bloom source white.
        glowBase = vec3(BLACK_NEON_GLOW_FLOOR);
        blackNeonGlowBoost = BLACK_NEON_GLOW_EXTRA_BOOST;
    }
    vec3 emissiveScene = albedo * emissive * EMISSIVE_SCENE_BOOST;
    vec3 glowColor = glowBase * emissive * EMISSIVE_GLOW_BOOST * blackNeonGlowBoost * glowMask;
    vec2 neonUv = gl_FragCoord.xy / vec2(max(textureSize(neonTexture, 0), ivec2(1)));
    vec3 neonSample = neonEnabled ? texture(neonTexture, neonUv).rgb : vec3(0.0);

    if (ignoreLighting > 0.5) {
        outSceneColor = vec4(albedo + emissiveScene + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(0.0);
        return;
    }

    // With black-neon enabled, emissive parts behave like unlit neon.
    // With black-neon disabled, keep physically plausible lit+emissive behavior.
    if (allowBlackNeon && emissive > 0.0) {
        outSceneColor = vec4(albedo + emissiveScene + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(glowColor, glowMask);
        return;
    }

    vec3 N = GetSurfaceMappedNormal(fragLocalNormal, surfaceType, faceUV);
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);

    if (camera.shadowState.w > 0.5) {
        vec3 color = (camera.ambient.rgb * camera.ambient.a) * albedo;
        vec3 L = normalize(-camera.lightDirection.xyz);
        float shadowFactor = ComputeShadowFactor(N, L);
        color += fragVertexLighting * surfaceDetail * shadowFactor;

        float reflectionWeight = clamp(metalness, 0.0, 1.0);
        if (reflectionWeight > 1e-4) {
            vec3 R = reflect(-V, N);
            vec3 env = texture(skyboxTex, R).rgb * camera.skyTint.rgb;
            if (dot(env, env) < 1e-6) {
                env = camera.skyTint.rgb;
            }
            vec3 F0 = mix(vec3(0.04), albedo, metalness);
            vec3 Fenv = FresnelSchlick(max(dot(N, V), 0.0), F0);
            float smoothness = 1.0 - roughness;
            color += env * Fenv * reflectionWeight * (smoothness * smoothness);
        }
        color += emissiveScene;
        outSceneColor = vec4(color + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(glowColor, glowMask);
        return;
    }

    vec3 color = (camera.ambient.rgb * camera.ambient.a) * albedo;

    vec3 L = normalize(-camera.lightDirection.xyz);
    vec3 H = normalize(V + L);
    float shadowFactor = ComputeShadowFactor(N, L);

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
    color += (diffuse + specular) * lightColor * NdotL * shadowFactor;

    float reflectionWeight = clamp(metalness, 0.0, 1.0);
    if (reflectionWeight > 1e-4) {
        vec3 R = reflect(-V, N);
        vec3 env = texture(skyboxTex, R).rgb * camera.skyTint.rgb;
        if (dot(env, env) < 1e-6) {
            env = camera.skyTint.rgb;
        }
        vec3 Fenv = FresnelSchlick(max(dot(N, V), 0.0), F0);
        color += env * Fenv * reflectionWeight * (smoothness * smoothness);
    }

    color += emissiveScene;
    outSceneColor = vec4(color + ((neonSample * 0.1) * glowMask), alpha);
    outGlowColor = vec4(glowColor, glowMask);
}
