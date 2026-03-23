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

const int SHADOW_SAMPLES_COUNT = 64;
const int SHADOW_SAMPLES_HALF = SHADOW_SAMPLES_COUNT / 2;
const float INV_SHADOW_SAMPLES = 1.0 / float(SHADOW_SAMPLES_COUNT);

vec2 GetDirectionalShadowTexelSize(int shadowMapBase, int cascadeIndex) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    ivec2 size = max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
    return 1.0 / vec2(size);
}

float SampleDirectionalShadowMap(int shadowMapBase, int cascadeIndex, vec3 shadowCoord) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return texture(directionalShadowMaps[idx], shadowCoord);
}

float SampleDirectionalPCF(DirectionalLight dl, int shadowMapBase, int cascadeIndex, vec3 baseShadowCoord, float radiusTexels, vec3 normal, vec3 lightDir) {
    int desiredTaps = clamp(int(dl.shadowParams.z + 0.5), 1, SHADOW_SAMPLES_COUNT);
    float radius = max(0.0, radiusTexels);
    if (radius <= 1e-6 || desiredTaps <= 1) {
        return SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, baseShadowCoord);
    }

    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    float texelScale = max(texelSize.x, texelSize.y);
    float fsize = radius * texelScale;

    float ndotl = clamp(dot(normal, lightDir), -1.0, 1.0);

    vec3 jcoord = vec3(gl_FragCoord.xy * dl.shadowState.yz, 0.0);
    float shadowSum = 0.0;

    // Cheap pretest: up to 8 taps via jitter fetches.
    int pretestTaps = min(8, desiredTaps);
    int tapsDone = 0;
    int pretestFetches = (pretestTaps + 1) / 2;
    for (int i = 0; i < pretestFetches; ++i) {
        vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
        jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

        vec3 smCoord = baseShadowCoord;
        if (tapsDone < pretestTaps) {
            vec2 uvOffset = offset.xy * fsize;
            smCoord.xy = uvOffset + baseShadowCoord.xy;
            shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
            tapsDone += 1;
        }

        if (tapsDone < pretestTaps) {
            vec2 uvOffset = offset.zw * fsize;
            smCoord.xy = uvOffset + baseShadowCoord.xy;
            shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
            tapsDone += 1;
        }
    }

    float shadowPretest = shadowSum / float(max(pretestTaps, 1));
    if (desiredTaps <= pretestTaps) {
        return shadowPretest;
    }

    if (ndotl > 0.0 && shadowPretest > 0.0 && shadowPretest < 1.0) {
        // Continue sampling until desired tap count (early-out keeps penumbra quality).
        for (int i = 0; i < SHADOW_SAMPLES_HALF && tapsDone < desiredTaps; ++i) {
            vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
            jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

            vec3 smCoord = baseShadowCoord;
            if (tapsDone < desiredTaps) {
                vec2 uvOffset = offset.xy * fsize;
                smCoord.xy = uvOffset + baseShadowCoord.xy;
                shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
                tapsDone += 1;
            }

            if (tapsDone < desiredTaps) {
                vec2 uvOffset = offset.zw * fsize;
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
    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    float constantBiasTexels = max(dl.shadowParams.x, 0.0);
    float slopeBiasFactor = max(dl.shadowBiasParams.x, 0.0);
    float maxBiasTexels = max(dl.shadowBiasParams.y, 0.0);
    float tanTheta = sqrt(max(1.0 - (ndotl * ndotl), 0.0)) / max(ndotl, 1e-3);
    float biasTexels = constantBiasTexels + (slopeBiasFactor * tanTheta);
    if (maxBiasTexels > 0.0) {
        biasTexels = min(biasTexels, maxBiasTexels);
    }
    return biasTexels * texelScale;
}

float ComputeDirectionalShadowFactor(DirectionalLight dl, int shadowMapBase, vec3 normal, vec3 lightDir) {
    if (dl.shadowState.x < 0.5) {
        return 1.0;
    }

    int cascadeCount = clamp(int(dl.shadowCascadeSplits.w + 0.5), 1, 3);
    float viewDepth = max(dot(fragWorldPos - camera.cameraPosition.xyz, normalize(camera.cameraForward.xyz)), 0.0);
    int cascadeIndex = 0;
    if (cascadeCount >= 3 && viewDepth > dl.shadowCascadeSplits.y) {
        cascadeIndex = 2;
    } else if (cascadeCount >= 2 && viewDepth > dl.shadowCascadeSplits.x) {
        cascadeIndex = 1;
    }

    vec4 lightClip = dl.shadowMatrices[cascadeIndex] * vec4(fragWorldPos, 1.0);
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
    float shadow = SampleDirectionalPCF(dl, shadowMapBase, cascadeIndex, vec3(shadowUv, compareDepth), softness, normal, lightDir);

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

    if (ignoreLighting) {
        outSceneColor = vec4(albedo + emissiveScene + ((neonSample * 0.1) * glowMask), alpha);
        outGlowColor = vec4(0.0);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }

    // With black-neon enabled, emissive parts behave like unlit neon.
    // With black-neon disabled, keep physically plausible lit+emissive behavior.
    if (allowBlackNeon && emissive > 0.0) {
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
                shadowFactor = ComputeDirectionalShadowFactor(dl, shadowMapBase, N, L);
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
                shadowFactor = ComputeDirectionalShadowFactor(dl, shadowMapBase, N, L);
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
