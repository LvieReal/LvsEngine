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
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings; // x: perVertexShadingEnabled
    vec4 cameraForward;
} camera;

layout(set = 0, binding = 1) uniform samplerCube skyboxTex;
layout(set = 0, binding = 2) uniform sampler2D surfaceAtlas;
layout(set = 0, binding = 3) uniform sampler2DShadow directionalShadowMaps[6];
layout(set = 0, binding = 13) uniform sampler2D neonTexture;
layout(set = 0, binding = 14) uniform sampler3D directionalShadowJitter;
layout(set = 0, binding = 15) uniform sampler2D surfaceNormalAtlas;


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

InstanceData inst;

layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outGlowColor;
layout(location = 2) out vec4 outDepthColor;


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

const float BLACK_NEON_GLOW_FLOOR = 1.0 / 255.0;
const int SMOOTH = 0;

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
        // Use normal matrix so non-uniform scale doesn't affect normal length/strength.
        mat3 normalMat = transpose(inverse(mat3(inst.model)));
        return normalize(normalMat * localNormal);
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

    // Build a stable world-space TBN; this keeps normal map "intensity" consistent across scaled meshes.
    mat3 modelMat = mat3(inst.model);
    mat3 normalMat = transpose(inverse(modelMat));
    vec3 normalWorld = normalize(normalMat * n);
    vec3 tangentWorld = normalize(modelMat * tangentLocal);
    vec3 bitangentWorld = normalize(cross(normalWorld, tangentWorld));
    tangentWorld = normalize(cross(bitangentWorld, normalWorld));

    vec3 mappedWorld = normalize(
        tangentWorld * tangentSpaceNormal.x +
        bitangentWorld * tangentSpaceNormal.y +
        normalWorld * tangentSpaceNormal.z
    );
    return mappedWorld;
}


// --- include begin: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\DirectionalShadows.glsl

#ifndef LVS_UTIL_DIRECTIONAL_SHADOWS_GLSL
#define LVS_UTIL_DIRECTIONAL_SHADOWS_GLSL

const int SHADOW_SAMPLES_COUNT = 64;
const int SHADOW_SAMPLES_HALF = SHADOW_SAMPLES_COUNT / 2;
const float INV_SHADOW_SAMPLES = 1.0 / float(SHADOW_SAMPLES_COUNT);

vec2 GetDirectionalShadowTexelSize(int shadowMapBase, int cascadeIndex) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    ivec2 size = max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
    return 1.0 / vec2(size);
}

ivec2 GetDirectionalShadowMapSize(int shadowMapBase, int cascadeIndex) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
}

float SampleDirectionalShadowMap(int shadowMapBase, int cascadeIndex, vec3 shadowCoord) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return texture(directionalShadowMaps[idx], shadowCoord);
}

float SampleDirectionalShadowHard(int shadowMapBase, int cascadeIndex, vec2 uv, float compareDepth) {
    vec2 cuv = clamp(uv, vec2(0.0), vec2(1.0));
    return SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, vec3(cuv, compareDepth)) > 0.5 ? 1.0 : 0.0;
}

float SampleDirectionalShadowHardTexel(int shadowMapBase, int cascadeIndex, ivec2 texel, ivec2 shadowSize, float compareDepth) {
    ivec2 clamped = clamp(texel, ivec2(0), shadowSize - ivec2(1));
    vec2 uv = (vec2(clamped) + 0.5) / vec2(shadowSize);
    return SampleDirectionalShadowHard(shadowMapBase, cascadeIndex, uv, compareDepth);
}

float ShadowTestRBSM(int shadowMapBase, int cascadeIndex, vec3 p) {
    return SampleDirectionalShadowHard(shadowMapBase, cascadeIndex, p.xy, p.z);
}

vec4 ComputeDiscontinuity4_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 o) {
    float center = ShadowTestRBSM(shadowMapBase, cascadeIndex, p);
    float left = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x - o.x, p.y, p.z));
    float right = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x + o.x, p.y, p.z));
    float bottom = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x, p.y - o.y, p.z));
    float top = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x, p.y + o.y, p.z));
    return abs(vec4(left, right, bottom, top) - center);
}

float ComputeRelativeDistance1_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 dir, float c, vec2 o) {
    const int maxSize = 16;
    vec3 np = p;
    float foundSilhouetteEnd = 0.0;
    float dist = 0.0;
    vec2 stepUv = dir * o;

    for (int it = 0; it < maxSize; ++it) {
        np.xy += stepUv;
        if (np.x < 0.0 || np.x > 1.0 || np.y < 0.0 || np.y > 1.0) {
            break;
        }

        float center = ShadowTestRBSM(shadowMapBase, cascadeIndex, np);
        bool isCenterUmbra = center < 0.5;
        if (isCenterUmbra) {
            foundSilhouetteEnd = 1.0;
            break;
        }

        vec4 d = ComputeDiscontinuity4_RBSM(shadowMapBase, cascadeIndex, np, o);
        if ((d.x + d.y + d.z + d.w) == 0.0) {
            break;
        }

        dist++;
    }

    dist += (1.0 - c);
    return mix(-dist, dist, foundSilhouetteEnd);
}

vec4 ComputeRelativeDistance4_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 c, vec2 o) {
    float dl = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, vec2(-1.0, 0.0), (1.0 - c.x), o);
    float dr = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, vec2(1.0, 0.0), c.x, o);
    vec2 bottomDir = vec2(0.0, -1.0);
    vec2 topDir = vec2(0.0, 1.0);
    float db = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, bottomDir, (1.0 - c.y), o);
    float dt = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, topDir, c.y, o);
    return vec4(dl, dr, db, dt);
}

float NormalizeRelativeDistance1_RBSM(vec2 dist) {
    const int maxSize = 16;
    float T = 1.0;
    if (dist.x < 0.0 && dist.y < 0.0) T = 0.0;
    if (dist.x > 0.0 && dist.y > 0.0) T = -2.0;
    float len = min(abs(dist.x) + abs(dist.y), float(maxSize));
    return (len > 1e-6) ? (abs(max(T * dist.x, T * dist.y)) / len) : 0.0;
}

vec2 NormalizeRelativeDistance2_RBSM(vec4 dist) {
    return vec2(
        NormalizeRelativeDistance1_RBSM(vec2(dist.x, dist.y)),
        NormalizeRelativeDistance1_RBSM(vec2(dist.z, dist.w))
    );
}

float RevectorizeShadow_RBSM(vec2 r) {
    // Conservative RBSM: return 0 for new shadow, 1 otherwise.
    if ((r.x * r.y > 0.0) && ((1.0 - r.x) > r.y)) return 0.0;
    return 1.0;
}

float SampleDirectionalRBSM(int shadowMapBase, int cascadeIndex, vec2 shadowUv, float compareDepth) {
    ivec2 shadowSize = GetDirectionalShadowMapSize(shadowMapBase, cascadeIndex);
    vec2 o = 1.0 / vec2(shadowSize);
    vec2 suv = clamp(shadowUv, vec2(0.0), vec2(1.0));
    vec3 p = vec3(suv, compareDepth);

    float shadowTestResult = ShadowTestRBSM(shadowMapBase, cascadeIndex, p);
    if (shadowTestResult < 0.5) {
        return 0.0;
    }

    vec2 c = fract(suv * vec2(shadowSize));
    vec4 disc = ComputeDiscontinuity4_RBSM(shadowMapBase, cascadeIndex, p, o);
    if ((disc.x + disc.y + disc.z + disc.w) > 0.0) {
        vec4 dist = ComputeRelativeDistance4_RBSM(shadowMapBase, cascadeIndex, p, c, o);
        vec2 r = NormalizeRelativeDistance2_RBSM(dist);
        return RevectorizeShadow_RBSM(r);
    }

    return shadowTestResult;
}

float SampleDirectionalPCF_RBSM(
    DirectionalLight dl,
    int shadowMapBase,
    int cascadeIndex,
    vec2 baseShadowUv,
    float compareDepth,
    float radiusTexels,
    vec2 fragCoord
) {
    int desiredTaps = clamp(int(dl.shadowParams.z + 0.5), 1, SHADOW_SAMPLES_COUNT);
    if (desiredTaps <= 1) {
        return SampleDirectionalRBSM(shadowMapBase, cascadeIndex, baseShadowUv, compareDepth);
    }

    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    float radius = max(radiusTexels, 0.0);

    vec3 jcoord = vec3(fragCoord * dl.shadowState.yz, 0.0);
    float shadowSum = 0.0;

    // Cheap pretest: up to 8 taps via jitter fetches.
    int pretestTaps = min(desiredTaps, 8);
    for (int i = 0; i < SHADOW_SAMPLES_HALF && (i * 2) < pretestTaps; ++i) {
        vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
        jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

        vec2 uvOffset = offset.xy * texelSize * radius;
        shadowSum += SampleDirectionalRBSM(shadowMapBase, cascadeIndex, baseShadowUv + uvOffset, compareDepth);

        if ((i * 2 + 1) < pretestTaps) {
            uvOffset = offset.zw * texelSize * radius;
            shadowSum += SampleDirectionalRBSM(shadowMapBase, cascadeIndex, baseShadowUv + uvOffset, compareDepth);
        }
    }

    float shadowPretest = shadowSum / float(max(pretestTaps, 1));
    if (desiredTaps <= pretestTaps) {
        return shadowPretest;
    }

    // Only refine near the boundary.
    if (shadowPretest > 0.0 && shadowPretest < 1.0) {
        int tapsDone = pretestTaps;
        for (int i = 0; i < SHADOW_SAMPLES_HALF && tapsDone < desiredTaps; ++i) {
            vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
            jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

            vec2 uvOffset = offset.xy * texelSize * radius;
            shadowSum += SampleDirectionalRBSM(shadowMapBase, cascadeIndex, baseShadowUv + uvOffset, compareDepth);
            tapsDone += 1;

            if (tapsDone < desiredTaps) {
                uvOffset = offset.zw * texelSize * radius;
                shadowSum += SampleDirectionalRBSM(shadowMapBase, cascadeIndex, baseShadowUv + uvOffset, compareDepth);
                tapsDone += 1;
            }
        }
        return shadowSum / float(desiredTaps);
    }

    return shadowPretest;
}

float SampleDirectionalPCF(
    DirectionalLight dl,
    int shadowMapBase,
    int cascadeIndex,
    vec3 baseShadowCoord,
    float radiusTexels,
    vec3 normal,
    vec3 lightDir,
    vec2 fragCoord
) {
    int desiredTaps = clamp(int(dl.shadowParams.z + 0.5), 1, SHADOW_SAMPLES_COUNT);
    if (desiredTaps <= 1) {
        return SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, baseShadowCoord);
    }

    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    float radius = max(radiusTexels, 0.0);

    float ndotl = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    vec3 jcoord = vec3(fragCoord * dl.shadowState.yz, 0.0);
    float shadowSum = 0.0;

    // Cheap pretest: up to 8 taps via jitter fetches.
    int pretestTaps = min(desiredTaps, 8);
    for (int i = 0; i < SHADOW_SAMPLES_HALF && (i * 2) < pretestTaps; ++i) {
        vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
        jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

        vec3 smCoord = baseShadowCoord;
        vec2 uvOffset = offset.xy * texelSize * radius;
        smCoord.xy = uvOffset + baseShadowCoord.xy;
        shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);

        if ((i * 2 + 1) < pretestTaps) {
            uvOffset = offset.zw * texelSize * radius;
            smCoord.xy = uvOffset + baseShadowCoord.xy;
            shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
        }
    }

    float shadowPretest = shadowSum / float(max(pretestTaps, 1));
    if (desiredTaps <= pretestTaps) {
        return shadowPretest;
    }

    // Only refine in the penumbra.
    if (ndotl > 0.0 && shadowPretest > 0.0 && shadowPretest < 1.0) {
        int tapsDone = pretestTaps;
        for (int i = 0; i < SHADOW_SAMPLES_HALF && tapsDone < desiredTaps; ++i) {
            vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
            jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

            vec3 smCoord = baseShadowCoord;
            vec2 uvOffset = offset.xy * texelSize * radius;
            smCoord.xy = uvOffset + baseShadowCoord.xy;
            shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
            tapsDone += 1;

            if (tapsDone < desiredTaps) {
                uvOffset = offset.zw * texelSize * radius;
                smCoord.xy = uvOffset + baseShadowCoord.xy;
                shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
                tapsDone += 1;
            }
        }
        return shadowSum / float(desiredTaps);
    }

    return shadowPretest;
}

float ComputeDirectionalBias(DirectionalLight dl, int shadowMapBase, int cascadeIndex, vec3 normal, vec3 lightDir) {
    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    float texelScale = max(texelSize.x, texelSize.y);

    float depthBiasTexels = max(dl.shadowParams.x, 0.0);
    return depthBiasTexels * texelScale;
}

vec3 ApplyDirectionalNormalOffset(
    DirectionalLight dl,
    int cascadeIndex,
    vec3 worldPos,
    vec3 normal,
    vec2 shadowUv,
    float receiverDepth,
    vec2 texelSize
) {
    float normalOffsetTexels = max(dl.shadowBiasParams.x, 0.0);
    if (normalOffsetTexels <= 1e-6) {
        return worldPos;
    }

    vec3 ndc0 = vec3((shadowUv * 2.0) - 1.0, receiverDepth);
    vec3 ndcX = vec3(((shadowUv + vec2(texelSize.x, 0.0)) * 2.0) - 1.0, receiverDepth);
    vec3 ndcY = vec3(((shadowUv + vec2(0.0, texelSize.y)) * 2.0) - 1.0, receiverDepth);

    vec4 w0 = dl.shadowInvMatrices[cascadeIndex] * vec4(ndc0, 1.0);
    vec4 wX = dl.shadowInvMatrices[cascadeIndex] * vec4(ndcX, 1.0);
    vec4 wY = dl.shadowInvMatrices[cascadeIndex] * vec4(ndcY, 1.0);

    float invW0 = 1.0 / max(abs(w0.w), 1e-6);
    float invWX = 1.0 / max(abs(wX.w), 1e-6);
    float invWY = 1.0 / max(abs(wY.w), 1e-6);

    vec3 p0 = w0.xyz * invW0;
    vec3 pX = wX.xyz * invWX;
    vec3 pY = wY.xyz * invWY;

    float worldPerTexel = max(length(pX - p0), length(pY - p0));
    float offsetWorld = normalOffsetTexels * worldPerTexel;
    return worldPos + (normalize(normal) * offsetWorld);
}

float ComputeDirectionalShadowFactor(
    DirectionalLight dl,
    int shadowMapBase,
    vec3 normal,
    vec3 lightDir,
    vec3 worldPos,
    vec3 cameraPos,
    vec3 cameraForward,
    vec2 fragCoord
) {
    if (dl.shadowState.x < 0.5) {
        return 1.0;
    }

    int cascadeCount = clamp(int(dl.shadowCascadeSplits.w + 0.5), 1, 3);
    vec3 fwd = normalize(cameraForward);
    float viewDepth = max(dot(worldPos - cameraPos, fwd), 0.0);
    int cascadeIndex = 0;
    if (cascadeCount >= 3 && viewDepth > dl.shadowCascadeSplits.y) {
        cascadeIndex = 2;
    } else if (cascadeCount >= 2 && viewDepth > dl.shadowCascadeSplits.x) {
        cascadeIndex = 1;
    }

    vec4 lightClip = dl.shadowMatrices[cascadeIndex] * vec4(worldPos, 1.0);
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

    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    vec3 biasedWorldPos = ApplyDirectionalNormalOffset(
        dl,
        cascadeIndex,
        worldPos,
        normal,
        shadowUv,
        receiverDepth,
        texelSize
    );
    if (distance(biasedWorldPos, worldPos) > 1e-7) {
        lightClip = dl.shadowMatrices[cascadeIndex] * vec4(biasedWorldPos, 1.0);
        if (abs(lightClip.w) <= 1e-6) {
            return 1.0;
        }
        lightNdc = lightClip.xyz / lightClip.w;
        shadowUv = (lightNdc.xy * 0.5) + 0.5;
        receiverDepth = lightNdc.z;
        if (receiverDepth <= 0.0 || receiverDepth >= 1.0) {
            return 1.0;
        }
        if (shadowUv.x < 0.0 || shadowUv.x > 1.0 || shadowUv.y < 0.0 || shadowUv.y > 1.0) {
            return 1.0;
        }
    }

    float softness = dl.shadowParams.y;
    vec2 baseSize = vec2(max(textureSize(directionalShadowMaps[clamp(shadowMapBase + 0, 0, 5)], 0), ivec2(1)));
    vec2 cascadeSize = baseSize;
    cascadeSize = vec2(max(textureSize(directionalShadowMaps[clamp(shadowMapBase + cascadeIndex, 0, 5)], 0), ivec2(1)));
    float baseRes = max(baseSize.x, baseSize.y);
    float cascadeRes = max(cascadeSize.x, cascadeSize.y);
    float resScale = cascadeRes / max(baseRes, 1.0);
    softness *= resScale;
    float bias = ComputeDirectionalBias(dl, shadowMapBase, cascadeIndex, normal, lightDir);
    float compareDepth = clamp(receiverDepth - bias, 0.0, 1.0);
    float shadow = 1.0;
    if (dl.shadowState.w > 0.5) {
        // RBSM silhouette recovery (hard) + optional PCF softening over the recovered silhouette.
        if (softness > 1e-4 && int(dl.shadowParams.z + 0.5) > 1) {
            shadow = SampleDirectionalPCF_RBSM(dl, shadowMapBase, cascadeIndex, shadowUv, compareDepth, softness, fragCoord);
        } else {
            shadow = SampleDirectionalRBSM(shadowMapBase, cascadeIndex, shadowUv, compareDepth);
        }
    } else {
        shadow = SampleDirectionalPCF(
            dl,
            shadowMapBase,
            cascadeIndex,
            vec3(shadowUv, compareDepth),
            softness,
            normal,
            lightDir,
            fragCoord
        );
    }

    if (cascadeIndex == cascadeCount - 1) {
        float fadeWidth = clamp(dl.shadowParams.w, 0.0, 1.0);
        if (fadeWidth > 1e-4) {
            float fadeStart = dl.shadowCascadeSplits.z * (1.0 - fadeWidth);
            float fade = clamp((viewDepth - fadeStart) / max(dl.shadowCascadeSplits.z - fadeStart, 1e-4), 0.0, 1.0);
            shadow = mix(shadow, 1.0, fade);
        }
    }
    return shadow;
}

#endif


// --- include end: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\DirectionalShadows.glsl

void main() {
    inst = instanceData.instances[fragInstanceIndex];

    vec3 albedo = fragBaseColor.rgb;
    float alpha = fragBaseColor.a;
    int surfaceType = GetSurfaceType(fragLocalNormal);
    vec2 faceUV = GetFaceUV(fragLocalPos, fragLocalNormal);
    vec3 surfaceDetail = SampleSurfaceColor(surfaceType, faceUV);
    albedo *= surfaceDetail;

    uint materialFlags = uint(fragMaterial.w + 0.5);
    bool ignoreLighting = (materialFlags & 1u) != 0u;
    bool excludeHbao = (materialFlags & 2u) != 0u;
    float hbaoDepth = excludeHbao ? 1.0 : gl_FragCoord.z;

    float metalness = clamp(fragMaterial.x, 0.0, 1.0);
    float roughness = clamp(fragMaterial.y, 0.0, 1.0);
    float emissive = max(fragMaterial.z, 0.0);
    bool neonEnabled = camera.renderSettings.z > 0.5;
    bool allowBlackNeon = (camera.renderSettings.w > 0.5) && neonEnabled;

    float albedoL2 = dot(albedo, albedo);
    float glowMask = (neonEnabled && !ignoreLighting && emissive > 1e-4) ? 1.0 : 0.0;
    if (!allowBlackNeon && albedoL2 < 1e-6) {
        glowMask = 0.0;
    }
    float whiteShift = allowBlackNeon ? 0.0 : smoothstep(1.0, 8.0, emissive);

    vec3 emissionRaw = albedo * emissive;
    float emissionMax = max(emissionRaw.r, max(emissionRaw.g, emissionRaw.b));
    vec3 emissionChroma = emissionRaw / max(emissionMax, 1e-6);
    vec3 emissionTint = mix(emissionChroma, vec3(1.0), whiteShift);
    vec3 emissiveScene = emissionTint * emissionMax;

    vec3 glowBase = albedo;
    if (allowBlackNeon && albedoL2 < 1e-6) {
        // Keep black-neon visible without turning the bloom source white.
        glowBase = vec3(BLACK_NEON_GLOW_FLOOR);
    }
    vec3 glowRaw = glowBase * emissive;
    float glowMax = max(glowRaw.r, max(glowRaw.g, glowRaw.b));
    vec3 glowChroma = glowRaw / max(glowMax, 1e-6);
    vec3 glowTint = mix(glowChroma, vec3(1.0), whiteShift);
    vec3 glowColor = (glowTint * glowMax) * glowMask;
    vec2 neonUv = gl_FragCoord.xy / vec2(max(textureSize(neonTexture, 0), ivec2(1)));
    vec3 neonSample = neonEnabled ? texture(neonTexture, neonUv).rgb : vec3(0.0);

    if (ignoreLighting) {
        outSceneColor = vec4(albedo + emissiveScene + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(0.0);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }

    // Emissive parts behave like unlit emitters (no diffuse/specular).
    if (emissive > 1e-4) {
        outSceneColor = vec4(albedo + emissiveScene + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(glowColor, glowMask);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }

    vec3 N = GetSurfaceMappedNormal(fragLocalNormal, surfaceType, faceUV);
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);

    if (camera.lightingSettings.x > 0.5) {
        vec3 color = (camera.ambient.rgb * camera.ambient.a) * albedo;

        float shadowFactor = 1.0;
        float fresnelAmount = 1.0;
        uint lightCount = min(lightData.counts.x, uint(64));
        for (uint i = 0; i < lightCount; ++i) {
            Light light = lightData.lights[i];
            if ((light.flags & 1u) == 0u) {
                continue;
            }
            fresnelAmount = clamp(light.specular.z, 0.0, 1.0);
            if (light.type == 0u && light.shadowIndex != 0xFFFFFFFFu) {
                DirectionalLight dl = lightData.directionalLights[light.dataIndex];
                vec3 L = normalize(-dl.direction.xyz);
                int shadowMapBase = int(light.shadowIndex) * 3;
                shadowFactor = ComputeDirectionalShadowFactor(
                    dl,
                    shadowMapBase,
                    N,
                    L,
                    fragWorldPos,
                    camera.cameraPosition.xyz,
                    camera.cameraForward.xyz,
                    gl_FragCoord.xy
                );
                break;
            }
        }
        color += fragVertexLighting * surfaceDetail * shadowFactor;

        float reflectionWeight = clamp(metalness, 0.0, 1.0);
        if (reflectionWeight > 1e-4) {
            vec3 R = reflect(-V, N);
            vec3 env = texture(skyboxTex, R).rgb * camera.skyTint.rgb;
            if (dot(env, env) < 1e-6) {
                env = camera.skyTint.rgb;
            }
            vec3 F0 = mix(vec3(0.04), albedo, metalness);
            vec3 FenvSchlick = FresnelSchlick(max(dot(N, V), 0.0), F0);
            vec3 Fenv = mix(F0, FenvSchlick, fresnelAmount);
            float smoothness = 1.0 - roughness;
            color += env * Fenv * reflectionWeight * (smoothness * smoothness);
        }
        color += emissiveScene;
        outSceneColor = vec4(color + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(glowColor, glowMask);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }

    vec3 color = (camera.ambient.rgb * camera.ambient.a) * albedo;

    float fresnelAmountEnv = 1.0;
    uint lightCount = min(lightData.counts.x, uint(64));
    for (uint i = 0; i < lightCount; ++i) {
        Light light = lightData.lights[i];
        if ((light.flags & 1u) == 0u) {
            continue;
        }
        fresnelAmountEnv = clamp(light.specular.z, 0.0, 1.0);
        break;
    }

    for (uint i = 0; i < lightCount; ++i) {
        Light light = lightData.lights[i];
        if ((light.flags & 1u) == 0u) {
            continue;
        }

        if (light.type == 0u) {
            DirectionalLight dl = lightData.directionalLights[light.dataIndex];
            vec3 L = normalize(-dl.direction.xyz);
            vec3 H = normalize(V + L);

            float shadowFactor = 1.0;
            if (light.shadowIndex != 0xFFFFFFFFu) {
                int shadowMapBase = int(light.shadowIndex) * 3;
                shadowFactor = ComputeDirectionalShadowFactor(
                    dl,
                    shadowMapBase,
                    N,
                    L,
                    fragWorldPos,
                    camera.cameraPosition.xyz,
                    camera.cameraForward.xyz,
                    gl_FragCoord.xy
                );
            }

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
            color += (diffuse + specular) * lightColor * NdotL * shadowFactor;
        }
    }

    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    float smoothness = 1.0 - roughness;
    float reflectionWeight = clamp(metalness, 0.0, 1.0);
    if (reflectionWeight > 1e-4) {
        vec3 R = reflect(-V, N);
        vec3 env = texture(skyboxTex, R).rgb * camera.skyTint.rgb;
        if (dot(env, env) < 1e-6) {
            env = camera.skyTint.rgb;
        }
        vec3 FenvSchlick = FresnelSchlick(max(dot(N, V), 0.0), F0);
        vec3 Fenv = mix(F0, FenvSchlick, fresnelAmountEnv);
        color += env * Fenv * reflectionWeight * (smoothness * smoothness);
    }

    color += emissiveScene;
    outSceneColor = vec4(color + ((neonSample * 0.1) * glowMask), alpha);
    outGlowColor = vec4(glowColor, glowMask);
    outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
}

